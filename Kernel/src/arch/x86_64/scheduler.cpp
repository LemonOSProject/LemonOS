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
#include <filesystem.h>
#include <abi.h>
#include <lock.h>

#define INITIAL_HANDLE_TABLE_SIZE 0xFFFF

uint64_t processPML4;

bool schedulerLock = true;

extern "C" void TaskSwitch(regs64_t* r);

extern "C"
uintptr_t ReadRIP();

extern "C"
void IdleProc();

namespace Scheduler{
    int lock;
    bool schedulerLock = true;

    process_t* processQueueStart = 0;
    process_t* currentProcess = 0;

    uint64_t nextPID = 0;

    handle_t handles[INITIAL_HANDLE_TABLE_SIZE];
    uint32_t handleCount = 1; // We don't want null handles
    uint32_t handleTableSize = INITIAL_HANDLE_TABLE_SIZE;

    void Initialize() {
        schedulerLock = true;
        memset(handles, 0, handleTableSize);
        CreateProcess((void*)IdleProc);

        processPML4 = currentProcess->addressSpace->pml4Phys;
        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)currentProcess->fxState) : "memory");

        asm("cli");
        Log::Write("OK");
        Memory::ChangeAddressSpace(currentProcess->addressSpace);

        TSS::SetKernelStack((uintptr_t)currentProcess->threads[0].kernelStack);

        schedulerLock = false;
        TaskSwitch(&currentProcess->threads[0].registers);
        for(;;);
    }

    process_t* GetCurrentProcess(){ return currentProcess; }

    handle_t RegisterHandle(void* pointer){
        handle_t handle = (handle_t)handleCount++;
        handles[(uint64_t)handle] = pointer;
        return handle;
    }

    void* FindHandle(handle_t handle){
        if((uintptr_t)handle < handleTableSize)
            return handles[(uint64_t)handle];
        else {
            Log::Warning("Invalid Handle! Process:");
            Log::Write((unsigned long)(currentProcess ? currentProcess->pid : -1));
        }
    }

    process_t* FindProcessByPID(uint64_t pid){
        if(!processQueueStart) return NULL;
        
        process_t* proc = processQueueStart;
        if(pid == proc->pid) return proc;
        proc = proc->next;

        while (proc != processQueueStart && proc){
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
        if(!processQueueStart){ // If queue is empty, add the process and link to itself
            processQueueStart = proc;
            proc->next = proc;
            currentProcess = proc;
        }
        else if(processQueueStart->next){ // More than 1 process in queue?
            proc->next = processQueueStart->next;
            processQueueStart->next = proc;
        } else { // If here should only be one process in queue
            processQueueStart->next = proc;
            proc->next = processQueueStart;
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
        currentProcess->timeSlice = 0;
    }

    process_t* CreateProcess(void* entry) {
        acquireLock(&lock);

        bool schedulerState = schedulerLock; // Get current value for scheduker lock
        schedulerLock = true; // Lock Scheduler

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

        schedulerLock = schedulerState; // Restore previous lock state

        releaseLock(&lock);

        return proc;
    }

    process_t* CreateELFProcess(void* elf, int argc, char** argv, int envc, char** envp) {
        if(!VerifyELF(elf)) return nullptr;

        bool schedulerState = schedulerLock; // Get current value for scheduker lock
        schedulerLock = true; // Lock Scheduler
        
        acquireLock(&lock);

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
            char* file = strtok(/*linkPath*/temp,"/");
            fs_node_t* node;
            fs_node_t* current_node = fs::GetRoot();
            while(file != NULL){
                node = current_node->findDir(current_node,file);
                if(!node) {
                    Log::Warning("Could not not find dynamic linker: ");
                    Log::Write(linkPath);
                    linkPath = nullptr;
                    continue;
                }
                if(node->flags & FS_NODE_DIRECTORY){
                    current_node = node;
                    file = strtok(NULL, "/");
                    Log::Warning(file);
                    continue;
                }
                break;
            }
            
            Log::Info("Dynamic Linker: ");
            Log::Write(node->name);

            void* linkerElf = kmalloc(node->size);
            fs::Read(node, 0, node->size, (uint8_t*)linkerElf); // Load Dynamic Linker
            
            if(!VerifyELF(linkerElf)){
                Log::Warning("Invalid Dynamic Linker ELF");
                releaseLock(&lock);
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

        *stack--;
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
        
        asm volatile("mov %%rax, %%cr3" :: "a"(currentProcess->addressSpace->pml4Phys));

        InsertProcessIntoQueue(proc);
        asm("sti");

        schedulerLock = schedulerState; // Restore previous lock state

        releaseLock(&lock);

        return proc;
    }

    void EndProcess(process_t* process){
        for(int i = 0; i < process->children.get_length(); i++){
            EndProcess(process->children.get_at(i));
        }
        asm("cli");
        acquireLock(&lock);

        if(process->parent){
            for(int i = 0; i < process->parent->children.get_length(); i++){
                if(process->parent->children[i] == process){
                    process->parent->children.remove_at(i);
                    break;
                }
            }
        }

        RemoveProcessFromQueue(process);

        if(currentProcess == process){
            asm volatile("mov %%rax, %%cr3" :: "a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE)); // If we are using the PML4 of the current process switch to the kernel's
        }

        for(int i = 0; i < process->sharedMemory.get_length(); i++){
            Memory::Free4KPages((void*)process->sharedMemory[i].base, process->sharedMemory[i].pageCount, process->addressSpace); // Make sure the physical memory does not get freed
        }

        Memory::DestroyAddressSpace(process->addressSpace);

        /*for(int i = 0; i < process->fileDescriptors.get_length(); i++){
            if(process->fileDescriptors[i]){
                fs::Close(process->fileDescriptors[i]);
            }
        }

        process->fileDescriptors.clear();

        /*for(int i = 0; i < process->programHeaders.get_length(); i++){
            elf32_program_header_t programHeader = process->programHeaders[i];

            for(int i = 0; i < ((programHeader.memSize / PAGE_SIZE) + 2); i++){
                uint32_t address = Memory::VirtualToPhysicalAddress(programHeader.vaddr + i * PAGE_SIZE);
                Memory::FreePhysicalMemoryBlock(address);
            }
        }*/
        if(currentProcess == process){
            currentProcess = process->next;
            kfree(process);
            processPML4 = currentProcess->addressSpace->pml4Phys;
            
            asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)currentProcess->fxState) : "memory");

            TSS::SetKernelStack((uintptr_t)currentProcess->threads[0].kernelStack);

            releaseLock(&lock);
            
            TaskSwitch(&currentProcess->threads[0].registers);
        }
        kfree(process);
        asm("sti");
        releaseLock(&lock);
    }

    void Tick(regs64_t* r){
        if(currentProcess && currentProcess->timeSlice > 0) {
            currentProcess->timeSlice--;
            return;
        }
        if(schedulerLock) return;
        
        currentProcess->timeSlice = currentProcess->timeSliceDefault;

        asm volatile ("fxsave64 (%0)" :: "r"((uintptr_t)currentProcess->fxState) : "memory");

        currentProcess->threads[0].registers = *r;

        currentProcess = currentProcess->next;

        if(!currentProcess) currentProcess = processQueueStart;

        processPML4 = currentProcess->addressSpace->pml4Phys;

        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)currentProcess->fxState) : "memory");

        TSS::SetKernelStack((uintptr_t)currentProcess->threads[0].kernelStack);
        
        TaskSwitch(&currentProcess->threads[0].registers);
        
        for(;;);
    }
}