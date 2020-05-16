#include <scheduler.h>

#include <paging.h>
#include <liballoc.h>
#include <physicalallocator.h>
#include <list.h>
#include <serial.h>
#include <idt.h>
#include <string.h>
#include <system.h>
#include <logging.h>
#include <elf.h>
#include <tss.h>
#include <fs/initrd.h>
#include <abi.h>
#include <lock.h>
#include <smp.h>
#include <apic.h>

#define INITIAL_HANDLE_TABLE_SIZE 0xFFFF

uint64_t processPML4;

extern "C" void TaskSwitch(regs64_t* r);

extern "C"
uintptr_t ReadRIP();

extern "C"
void IdleProc();

namespace Scheduler{
    int schedulerLock = 0;
    bool schedulerReady = false;

    process_t* processes;
    unsigned processTableSize = 512;
    uint64_t nextPID = 0;

    process_t* readyQueue = nullptr;
    thread_t* ranQueue = nullptr;

    handle_t handles[INITIAL_HANDLE_TABLE_SIZE];
    uint64_t handleCount = 1; // We don't want null handles
    uint32_t handleTableSize = INITIAL_HANDLE_TABLE_SIZE;
    
    void Schedule(regs64_t* r);

    void Initialize() {
        memset(handles, 0, handleTableSize);

        processes = (process_t*)kmalloc(sizeof(process_t*));

        CPU* cpu = GetCPULocal();
        Log::Info("CPU local: %x", cpu);

        process_t* proc = CreateProcess((void*)IdleProc);
        cpu->currentProcess = proc;
        
        processPML4 = cpu->currentProcess->addressSpace->pml4Phys;
        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)cpu->currentProcess->fxState) : "memory");

        asm("cli");
        IDT::RegisterInterruptHandler(IPI_SCHEDULE, Schedule);

        TSS::SetKernelStack(&SMP::cpus[0]->tss, (uintptr_t)cpu->currentProcess->threads[0].kernelStack);

        schedulerReady = true;
        TaskSwitch(&cpu->currentProcess->threads[0].registers);
        for(;;);
    }

    process_t* GetCurrentProcess(){ 
        CPU* cpu = GetCPULocal();
        return cpu->currentProcess;
    }

    handle_t RegisterHandle(void* pointer){
        handle_t handle = (handle_t)(handleCount++);
        handles[(uint64_t)handle] = pointer;

        return handle;
    }

    void* FindHandle(handle_t handle){
        if((uintptr_t)handle < handleTableSize || !handle)
            return handles[(uint64_t)handle];
        else {
            //Log::Warning("Invalid Handle: %x, Process: %d", (uintptr_t)handle, (unsigned long)(currentProcess ? currentProcess->pid : -1));
            return nullptr;
        }
    }

    process_t* FindProcessByPID(uint64_t pid){
        if(!readyQueue) return NULL;
        
        process_t* proc = readyQueue;
        if(pid == proc->pid) return proc;
        proc = proc->next;

        while (proc != readyQueue && proc){
            if(pid == proc->pid) return proc;
            proc = proc->next;
        }
        return NULL;
    }

    int SendMessage(message_t msg){
        process_t* proc = FindProcessByPID(msg.recieverPID);
        if(!proc) return 1; // Failed to find process with specified PID
        proc->messageQueue.add_back(msg);
        return 0; // Success
    }

    int SendMessage(process_t* proc, message_t msg){
        proc->messageQueue.add_back(msg); // Add message to queue
        return 0; // Success
    }

    message_t RecieveMessage(process_t* proc){

        if(proc->messageQueue.get_length() <= 0 || !proc){
            message_t nullMsg;
            nullMsg.senderPID = 0;
            nullMsg.recieverPID = 0; // Idle process should not be asking for messages
            nullMsg.msg = 0;
            return nullMsg;
        }
        return proc->messageQueue.remove_at(0);
    }

    void InsertProcessIntoQueue(process_t* proc){
        CPU* cpu = GetCPULocal();
        if(!readyQueue){ // If queue is empty, add the process and link to itself
            readyQueue = proc;
            proc->next = proc;
            cpu->currentProcess = proc;
        }
        else if(readyQueue->next){ // More than 1 process in queue?
            proc->next = readyQueue->next;
            readyQueue->next = proc;
        } else { // If here should only be one process in queue
            readyQueue->next = proc;
            proc->next = readyQueue;
        }
    }

    void RemoveProcessFromQueue(process_t* proc){
        process_t* _proc = proc->next;
        process_t* nextProc = proc->next;

        while(_proc->next && _proc != proc){
            if(_proc->next == proc){
                _proc->next = nextProc;
                break;
            }

            _proc = _proc->next;
        }
    }

    process_t* InitializeProcessStructure(){

        // Create process structure
        process_t* proc = (process_t*)kmalloc(sizeof(process_t));

        memset(proc,0,sizeof(process_t));

        proc->fileDescriptors.clear();

        // Reserve 3 file descriptors for stdin, out and err
        fs_node_t* nullDev = fs::ResolvePath("/dev/null");
        proc->fileDescriptors.add_back(fs::Open(nullDev));  //(NULL);
        proc->fileDescriptors.add_back(fs::Open(nullDev));  //(NULL);
        proc->fileDescriptors.add_back(fs::Open(nullDev));  //(NULL);

        proc->pid = nextPID++; // Set Process ID to the next availiable
        proc->priority = 1;
        proc->thread_count = 1;
        proc->state = PROCESS_STATE_ACTIVE;
        proc->thread_count = 1;

        proc->addressSpace = Memory::CreateAddressSpace();// So far this function is only used for idle task, we don't need an address space
        proc->timeSliceDefault = 1;
        proc->timeSlice = proc->timeSliceDefault;

        proc->next = NULL;

        // Create structure for the main thread
        thread_t* thread = proc->threads;

        thread->stack = 0;
        thread->parent = proc;
        thread->priority = 1;
        thread->parent = proc;

        regs64_t* registers = &thread->registers;
        memset((uint8_t*)registers, 0, sizeof(regs64_t));
        registers->rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
        registers->cs = 0x08; // Kernel CS
        registers->ss = 0x10; // Kernel SS

        proc->fxState = Memory::KernelAllocate4KPages(1); // Allocate Memory for the FPU/Extended Register State
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)proc->fxState, 1);
        memset(proc->fxState, 0, 1024);

        void* kernelStack = (void*)Memory::KernelAllocate4KPages(24); // Allocate Memory For Kernel Stack
        for(int i = 0; i < 24; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)kernelStack + PAGE_SIZE_4K * i, 1);
        }
        thread->kernelStack = kernelStack + PAGE_SIZE_4K * 24;

        ((fx_state_t*)proc->fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
        ((fx_state_t*)proc->fxState)->mxcsrMask = 0xffbf;
        ((fx_state_t*)proc->fxState)->fcw = 0x33f; // Default FPU Control Word State

        strcpy(proc->workingDir, "/"); // set root as default working dir

        return proc;
    }

    void Yield(){
        //currentProcess->timeSlice = 0;
    }

    process_t* CreateProcess(void* entry) {
        acquireLock(&schedulerLock);

        process_t* proc = InitializeProcessStructure();
        thread_t* thread = &proc->threads[0];

        void* stack = (void*)Memory::KernelAllocate4KPages(32);//, proc->addressSpace);
        for(int i = 0; i < 32; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)stack + PAGE_SIZE_4K * i, 1);//, proc->addressSpace);
        }

        thread->stack = stack + PAGE_SIZE_4K * 32; // 128KB stack size
        thread->registers.rsp = (uintptr_t)thread->stack;
        thread->registers.rbp = (uintptr_t)thread->stack;
        thread->registers.rip = (uintptr_t)entry;

        InsertProcessIntoQueue(proc);

        releaseLock(&schedulerLock);

        return proc;
    }

    void EndProcess(process_t* process){
        for(unsigned i = 0; i < process->children.get_length(); i++){
            EndProcess(process->children.get_at(i));
        }
        asm("cli");
        acquireLock(&schedulerLock);

        if(process->parent){
            for(unsigned i = 0; i < process->parent->children.get_length(); i++){
                if(process->parent->children[i] == process){
                    process->parent->children.remove_at(i);
                    break;
                }
            }
        }

        RemoveProcessFromQueue(process);

        process_t* currentProc = SMP::cpus[0]->currentProcess;
        if(SMP::cpus[0]->currentProcess == process){
            asm volatile("mov %%rax, %%cr3" :: "a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE)); // If we are using the PML4 of the current process switch to the kernel's
        }

        for(unsigned i = 0; i < process->sharedMemory.get_length(); i++){
            Memory::Free4KPages((void*)process->sharedMemory[i].base, process->sharedMemory[i].pageCount, process->addressSpace); // Make sure the physical memory does not get freed
        }

        Memory::DestroyAddressSpace(process->addressSpace);

        /*for(int i = 0; i < process->fileDescriptors.get_length(); i++){
            if(process->fileDescriptors[i]){
                fs::Close(process->fileDescriptors[i]);
            }
        }

        process->fileDescriptors.clear();*/

        if(currentProc == process){
            currentProc = process->next;
            kfree(process);
            processPML4 = currentProc->addressSpace->pml4Phys;
            
            asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)currentProc->fxState) : "memory");

            TSS::SetKernelStack(&SMP::cpus[0]->tss, (uintptr_t)currentProc->threads[0].kernelStack);

            releaseLock(&schedulerLock);
            
            TaskSwitch(&currentProc->threads[0].registers);
        }
        kfree(process);
        asm("sti");
        releaseLock(&schedulerLock);
    }

    void Tick(regs64_t* r){
        if(!schedulerReady) return;

        APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);

        Schedule(r);
    }

    void Schedule(regs64_t* r){

        CPU* cpu = GetCPULocal();

        if(cpu->id != 0) return;

        if(cpu->currentProcess && cpu->currentProcess->timeSlice > 0) {
            cpu->currentProcess->timeSlice--;
            return;
        }
        
        acquireLock(&schedulerLock);
        
        cpu->currentProcess->timeSlice = cpu->currentProcess->timeSliceDefault;

        asm volatile ("fxsave64 (%0)" :: "r"((uintptr_t)cpu->currentProcess->fxState) : "memory");

        cpu->currentProcess->threads[0].registers = *r;


        if(!cpu->currentProcess) {
            cpu->currentProcess = readyQueue;
        } else {
            Log::Info("p: %x", cpu->currentProcess);
            cpu->currentProcess = cpu->currentProcess->next;
        }

        processPML4 = cpu->currentProcess->addressSpace->pml4Phys;

        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)cpu->currentProcess->fxState) : "memory");

        releaseLock(&schedulerLock);

        asm volatile("mov %%rax, %%cr8" :: "a"(cpu->currentProcess->priority)); // Set Task Priority Register
        TSS::SetKernelStack(&cpu->tss, (uintptr_t)cpu->currentProcess->threads[0].kernelStack);
        
        TaskSwitch(&cpu->currentProcess->threads[0].registers);
        
        for(;;);
    }

    process_t* CreateELFProcess(void* elf, int argc, char** argv, int envc, char** envp) {
        if(!VerifyELF(elf)) return nullptr;

        // Create process structure
        process_t* proc = InitializeProcessStructure();

        proc->timeSliceDefault = 4;
        proc->timeSlice = proc->timeSliceDefault;

        thread_t* thread = &proc->threads[0];
        thread->registers.cs = 0x1B; // We want user mode so use user mode segments, make sure RPL is 3
        thread->registers.ss = 0x23;

        Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),0,1,proc->addressSpace);
        
        asm("cli");

        asm volatile("mov %%rax, %%cr3" :: "a"(proc->addressSpace->pml4Phys));

        elf_info_t elfInfo = LoadELFSegments(proc, elf, 0);
        
        thread->registers.rip = elfInfo.entry;

        if(elfInfo.linkerPath){
            char* linkPath = elfInfo.linkerPath;
            uintptr_t linkerBaseAddress = 0x7FC0000000; // Linker base address

            char temp[32];
            strcpy(temp, "/initrd/ld.so");
            fs_node_t* node = fs::ResolvePath(temp);
            
            Log::Info("Dynamic Linker: ");
            Log::Write(node->name);

            void* linkerElf = kmalloc(node->size);
            fs::Read(node, 0, node->size, (uint8_t*)linkerElf); // Load Dynamic Linker
            
            if(!VerifyELF(linkerElf)){
                Log::Warning("Invalid Dynamic Linker ELF");
                asm volatile("mov %%rax, %%cr3" :: "a"(GetCurrentProcess()->addressSpace->pml4Phys));
                asm("sti");
                return nullptr;
            }

            elf_info_t linkerELFInfo = LoadELFSegments(proc, linkerElf, linkerBaseAddress);

            thread->registers.rip = linkerELFInfo.entry;
        }

        void* _stack = (void*)Memory::Allocate4KPages(32, proc->addressSpace);
        for(int i = 0; i < 32; i++){
            Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)_stack + PAGE_SIZE_4K * i, 1, proc->addressSpace);
        }
        memset(_stack, 0, PAGE_SIZE_4K * 32);

        thread->stack = _stack + PAGE_SIZE_4K * 32; // 128KB stack size
        thread->registers.rsp = (uintptr_t)thread->stack;
        thread->registers.rbp = (uintptr_t)thread->stack;

        // ABI Stuff
        uint64_t* stack = (uint64_t*)thread->registers.rsp;

        char** tempArgv = (char**)kmalloc(argc * sizeof(char*));

        char* stackStr = (char*)stack;
        for(int i = 0; i < argc; i++){
            stackStr -= strlen(argv[i]) + 1;
            tempArgv[i] = stackStr;
            strcpy((char*)stackStr, argv[i]);
        }

        char** tempEnvp = (char**)kmalloc((envc) * sizeof(char*));
        if(envp){
            for(int i = 0; i < envc; i++){
                stackStr -= strlen(envp[i]) + 1;
                tempEnvp[i] = stackStr;
                strcpy((char*)stackStr, envp[i]);
            }
        }

        stackStr -= (uintptr_t)stackStr & 0xf; // align the stack

        stack = (uint64_t*)stackStr;

        stack -= ((argc + envc) % 2); // If argc + envCount is odd then the stack will be misaligned

        stack--;
        *stack = 0; // AT_NULL

        stack -= sizeof(auxv_t)/sizeof(*stack);
        *((auxv_t*)stack) = {.a_type = AT_PHDR, .a_val = elfInfo.pHdrSegment}; // AT_PHDR
        
        stack -= sizeof(auxv_t)/sizeof(*stack);
        *((auxv_t*)stack) = {.a_type = AT_PHENT, .a_val = elfInfo.phEntrySize}; // AT_PHENT
        
        stack -= sizeof(auxv_t)/sizeof(*stack);
        *((auxv_t*)stack) = {.a_type = AT_PHNUM, .a_val = elfInfo.phNum}; // AT_PHNUM

        stack -= sizeof(auxv_t)/sizeof(*stack);
        *((auxv_t*)stack) = {.a_type = AT_ENTRY, .a_val = elfInfo.entry}; // AT_ENTRY

        stack--;
        *stack = 0; // null

        stack -= envc;
        for(int i = 0; i < envc; i++){
            *(stack + i) = (uint64_t)tempEnvp[i];
        }

        stack--;
        *stack = 0; // null

        stack -= argc;
        for(int i = 0; i < argc; i++){
            *(stack + i) = (uint64_t)tempArgv[i];
        }

        stack--;
        *stack = argc; // argc

        thread->registers.rsp = (uintptr_t) stack;
        thread->registers.rbp = (uintptr_t) stack;
        
        Log::Info("Entry: %x, Stack: %x", thread->registers.rip, stack);
        
        asm volatile("mov %%rax, %%cr3" :: "a"(GetCurrentProcess()->addressSpace->pml4Phys));

        acquireLock(&schedulerLock);

        InsertProcessIntoQueue(proc);

        releaseLock(&schedulerLock);
        asm("sti");

        return proc;
    }
}