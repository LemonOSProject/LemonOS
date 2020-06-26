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

extern "C" void TaskSwitch(regs64_t* r, uint64_t pml4);

extern "C"
void IdleProc();

void KernelProcess();

namespace Scheduler{
    int schedulerLock = 0;
    bool schedulerReady = false;

    List<process_t*>* processes;
    unsigned processTableSize = 512;
    uint64_t nextPID = 0;

    process_t* idleProcess;

    handle_t handles[INITIAL_HANDLE_TABLE_SIZE];
    uint64_t handleCount = 1; // We don't want null handles
    uint32_t handleTableSize = INITIAL_HANDLE_TABLE_SIZE;
    
    void Schedule(regs64_t* r);
    
    inline void InsertThreadIntoQueue(thread_t* thread){
        GetCPULocal()->runQueue->add_back(thread);
    }

    inline void RemoveThreadFromQueue(thread_t* thread){
        GetCPULocal()->runQueue->remove(thread);
    }
    
    void InsertNewThreadIntoQueue(thread_t* thread){
        CPU* cpu = SMP::cpus[0];
        for(unsigned i = 0; i < SMP::processorCount; i++){
            Log::Info("CPU %d has %d threads", i, SMP::cpus[i]->runQueue->get_length());
            
            if(SMP::cpus[i]->runQueue->get_length() < cpu->runQueue->get_length()) {
                cpu = SMP::cpus[i];
            }
        }

        Log::Info("Inserting thread into run queue of CPU %d", cpu->id);

        acquireLock(&cpu->runQueueLock);
        asm("cli");
        cpu->runQueue->add_back(thread);
        asm("sti");
        releaseLock(&cpu->runQueueLock);
    }

    void Initialize() {
        memset(handles, 0, handleTableSize);

        processes = new List<process_t*>();

        CPU* cpu = GetCPULocal();

        idleProcess = CreateProcess((void*)IdleProc);

        for(unsigned i = 0; i < SMP::processorCount; i++) SMP::cpus[i]->runQueue->clear();
        
        IDT::RegisterInterruptHandler(IPI_SCHEDULE, Schedule);

        CreateProcess((void*)KernelProcess);

        cpu->currentThread = nullptr;
        schedulerReady = true;
        asm("sti");
        for(;;);
    }

    process_t* GetCurrentProcess(){ 
        asm("cli");
        CPU* cpu = GetCPULocal();

        process_t* ret = nullptr;

        if(cpu->currentThread)
            ret = cpu->currentThread->parent;

        asm("sti");
        return ret;
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
        for(unsigned i = 0; i < processes->get_length(); i++){
            if(processes->get_at(i)->pid == pid) return processes->get_at(i);
        }

        return nullptr;
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

    process_t* InitializeProcessStructure(){

        // Create process structure
        process_t* proc = (process_t*)kmalloc(sizeof(process_t));

        memset(proc,0,sizeof(process_t));

        proc->fileDescriptors.clear();
        proc->sharedMemory.clear();
        proc->children.clear();

        proc->threads = (thread_t*)kmalloc(sizeof(thread_t) * 1);

        // Reserve 3 file descriptors for stdin, out and err
        FsNode* nullDev = fs::ResolvePath("/dev/null");
        FsNode* logDev = fs::ResolvePath("/dev/kernellog");
        proc->fileDescriptors.add_back(fs::Open(nullDev));          //(NULL);
        proc->fileDescriptors.add_back(fs::Open(logDev));   //(fs::Open(nullDev));  //(NULL);
        proc->fileDescriptors.add_back(fs::Open(nullDev));          //(NULL);
        proc->state = PROCESS_STATE_ACTIVE;
        proc->threadCount = 1;
        proc->parent = nullptr;
        proc->uid = 0;

        proc->addressSpace = Memory::CreateAddressSpace();
        proc->pid = nextPID++; // Set Process ID to the next availiable

        // Create structure for the main thread
        thread_t* thread = proc->threads;

        thread->stack = 0;
        thread->priority = 1;
        thread->timeSliceDefault = 1;
        thread->timeSlice = thread->timeSliceDefault;
        thread->fsBase = 0;

        thread->next = nullptr;
        thread->prev = nullptr;
        thread->parent = proc;

        regs64_t* registers = &thread->registers;
        memset((uint8_t*)registers, 0, sizeof(regs64_t));
        registers->rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
        registers->cs = 0x08; // Kernel CS
        registers->ss = 0x10; // Kernel SS

        thread->fxState = Memory::KernelAllocate4KPages(1); // Allocate Memory for the FPU/Extended Register State
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)thread->fxState, 1);
        memset(thread->fxState, 0, 1024);

        void* kernelStack = (void*)Memory::KernelAllocate4KPages(24); // Allocate Memory For Kernel Stack (96KB)
        for(int i = 0; i < 24; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)kernelStack + PAGE_SIZE_4K * i, 1);
        }
        thread->kernelStack = kernelStack + PAGE_SIZE_4K * 24;

        ((fx_state_t*)thread->fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
        ((fx_state_t*)thread->fxState)->mxcsrMask = 0xffbf;
        ((fx_state_t*)thread->fxState)->fcw = 0x33f; // Default FPU Control Word State

        strcpy(proc->workingDir, "/"); // set root as default working dir

        return proc;
    }

    void Yield(){
        CPU* cpu = GetCPULocal();
        
        if(cpu->currentThread) {
            cpu->currentThread->timeSlice = 0;
            APIC::Local::SendIPI(0, ICR_DSH_SELF, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE); // Send schedule IPI to self
        }
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

        InsertNewThreadIntoQueue(&proc->threads[0]);

        processes->add_back(proc);

        releaseLock(&schedulerLock);

        return proc;
    }

    void EndProcess(process_t* process){
        for(unsigned i = 0; i < process->children.get_length(); i++){
            EndProcess(process->children.get_at(i));
        }

        if(process->parent){
            for(unsigned i = 0; i < process->parent->children.get_length(); i++){
                if(process->parent->children[i] == process){
                    process->parent->children.remove_at(i);
                    break;
                }
            }
        }

        CPU* cpu = GetCPULocal();
        asm("sti");
        acquireLock(&cpu->runQueueLock);
        asm("cli");

        for(uint32_t i = 0; i < process->threadCount; i++){
            for(unsigned j = 0; j < cpu->runQueue->get_length(); j++){
                if(cpu->runQueue->get_at(j) == &process->threads[i]) cpu->runQueue->remove(&process->threads[i]);
            }
        }

        for(unsigned i = 0; i < processes->get_length(); i++){
            if(processes->get_at(i) == process)
                processes->remove_at(i);
        }

        for(unsigned i = 0; i < process->fileDescriptors.get_length(); i++){
            if(process->fileDescriptors[i]){
                fs::Close(process->fileDescriptors[i]);
            }
        }

        process->fileDescriptors.clear();
        
        for(unsigned i = 0; i < SMP::processorCount; i++){
            if(i == cpu->id) continue; // Is current processor?

            acquireLock(&SMP::cpus[i]->runQueueLock);

            if(SMP::cpus[i]->currentThread->parent == process){
                SMP::cpus[i]->currentThread = nullptr;
                APIC::Local::SendIPI(i, 0, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);
            }
            
            for(unsigned j = 0; j < SMP::cpus[i]->runQueue->get_length(); j++){
                thread_t* thread = SMP::cpus[i]->runQueue->get_at(j);

                assert(thread);

                if(thread->parent == process){
                    SMP::cpus[i]->runQueue->remove(thread);
                }
            }
            
            releaseLock(&SMP::cpus[i]->runQueueLock);
        }

        if(cpu->currentThread->parent == process){
            asm volatile("mov %%rax, %%cr3" :: "a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE)); // If we are using the PML4 of the current process switch to the kernel's
        }

        for(unsigned i = 0; i < process->sharedMemory.get_length(); i++){
            Memory::Free4KPages((void*)process->sharedMemory[i].base, process->sharedMemory[i].pageCount, process->addressSpace); // Make sure the physical memory does not get freed
        }

        Memory::DestroyAddressSpace(process->addressSpace);

        if(cpu->currentThread->parent == process){
            cpu->currentThread = nullptr; // Force reschedule
            kfree(process->threads);
            kfree(process);
            releaseLock(&cpu->runQueueLock);

            Schedule(nullptr);

            asm("sti");
            for(;;) {
                asm("hlt");
                Schedule(nullptr);
            }
        }

        releaseLock(&cpu->runQueueLock);
        kfree(process);
        asm("sti");
    }

    void Tick(regs64_t* r){
        if(!schedulerReady) return;

        APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);

        Schedule(r);
    }

    void Schedule(regs64_t* r){
        CPU* cpu = GetCPULocal();

        if(cpu->currentThread && cpu->currentThread->timeSlice > 0) {
            cpu->currentThread->timeSlice--;
            return;
        }

        while(__builtin_expect(acquireTestLock(&cpu->runQueueLock), 0)) {
            return;
        }
        
        if(__builtin_expect(cpu->currentThread && cpu->currentThread->parent != idleProcess, 1)){
            cpu->currentThread->timeSlice = cpu->currentThread->timeSliceDefault;

            asm volatile ("fxsave64 (%0)" :: "r"((uintptr_t)cpu->currentThread->fxState) : "memory");

            cpu->currentThread->registers = *r;

            cpu->runQueue->add_back(cpu->currentThread);
        }
        
        if (__builtin_expect(cpu->runQueue->get_length() <= 0, 0)){
            cpu->currentThread = &idleProcess->threads[0];
        } else {
            cpu->currentThread = cpu->runQueue->remove_at(0);
        }
        releaseLock(&cpu->runQueueLock);

        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)cpu->currentThread->fxState) : "memory");

	    asm volatile ("wrmsr" :: "a"(cpu->currentThread->fsBase & 0xFFFFFFFF) /*Value low*/, "d"((cpu->currentThread->fsBase >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);
        
        TSS::SetKernelStack(&cpu->tss, (uintptr_t)cpu->currentThread->kernelStack);

        TaskSwitch(&cpu->currentThread->registers, cpu->currentThread->parent->addressSpace->pml4Phys);
        
        for(;;);
    }

    process_t* CreateELFProcess(void* elf, int argc, char** argv, int envc, char** envp) {
        if(!VerifyELF(elf)) return nullptr;

        // Create process structure
        process_t* proc = InitializeProcessStructure();

        thread_t* thread = &proc->threads[0];
        thread->registers.cs = 0x1B; // We want user mode so use user mode segments, make sure RPL is 3
        thread->registers.ss = 0x23;
        thread->timeSliceDefault = 5;
        thread->timeSlice = thread->timeSliceDefault;
        thread->priority = 4;

        Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),0,1,proc->addressSpace);

        elf_info_t elfInfo = LoadELFSegments(proc, elf, 0);
        
        thread->registers.rip = elfInfo.entry;

        if(elfInfo.linkerPath){
            char* linkPath = elfInfo.linkerPath;
            uintptr_t linkerBaseAddress = 0x7FC0000000; // Linker base address

            char temp[32];
            strcpy(temp, "/initrd/ld.so");
            FsNode* node = fs::ResolvePath(temp);
            
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

        char** tempArgv = (char**)kmalloc(argc * sizeof(char*));
        char** tempEnvp = (char**)kmalloc((envc) * sizeof(char*));

        asm("cli");
        asm volatile("mov %%rax, %%cr3" :: "a"(proc->addressSpace->pml4Phys));
        void* _stack = (void*)Memory::Allocate4KPages(48, proc->addressSpace);
        for(int i = 0; i < 48; i++){
            Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)_stack + PAGE_SIZE_4K * i, 1, proc->addressSpace);
        }
        memset(_stack, 0, PAGE_SIZE_4K * 48);

        thread->stack = _stack + PAGE_SIZE_4K * 48; // 192KB stack size
        thread->registers.rsp = (uintptr_t)thread->stack;
        thread->registers.rbp = (uintptr_t)thread->stack;

        // ABI Stuff
        uint64_t* stack = (uint64_t*)thread->registers.rsp;

        char* stackStr = (char*)stack;
        for(int i = 0; i < argc; i++){
            stackStr -= strlen(argv[i]) + 1;
            tempArgv[i] = stackStr;
            strcpy((char*)stackStr, argv[i]);
        }

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
        
        asm volatile("mov %%rax, %%cr3" :: "a"(GetCurrentProcess()->addressSpace->pml4Phys));
        asm("sti");

        kfree(tempArgv);
        kfree(tempEnvp);

        thread->registers.rsp = (uintptr_t) stack;
        thread->registers.rbp = (uintptr_t) stack;
        
        assert(!(thread->registers.rsp & 0xF));

        Log::Info("Entry: %x, Stack: %x", thread->registers.rip, stack);

        processes->add_back(proc);

        InsertNewThreadIntoQueue(&proc->threads[0]);

        return proc;
    }
}