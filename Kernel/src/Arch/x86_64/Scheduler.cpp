#include <Scheduler.h>

#include <Paging.h>
#include <Liballoc.h>
#include <PhysicalAllocator.h>
#include <List.h>
#include <Serial.h>
#include <IDT.h>
#include <String.h>
#include <System.h>
#include <Logging.h>
#include <ELF.h>
#include <TSS.h>
#include <Fs/Initrd.h>
#include <ABI.h>
#include <Lock.h>
#include <SMP.h>
#include <APIC.h>
#include <Timer.h>
#include <Debug.h>

extern "C" [[noreturn]] void TaskSwitch(RegisterContext* r, uint64_t pml4);

extern "C"
void IdleProcess();

void KernelProcess();

namespace Scheduler{
    int schedulerLock = 0;
    bool schedulerReady = false;

    List<process_t*>* processes;

    lock_t destroyedProcessesLock = 0;
    List<process_t*>* destroyedProcesses;
    
    unsigned processTableSize = 512;
    uint64_t nextPID = 1;

    void Schedule(void*, RegisterContext* r);
    
    inline void InsertThreadIntoQueue(Thread* thread){
        GetCPULocal()->runQueue->add_back(thread);
    }

    inline void RemoveThreadFromQueue(Thread* thread){
        GetCPULocal()->runQueue->remove(thread);
    }
    
    void InsertNewThreadIntoQueue(Thread* thread){
        CPU* cpu = SMP::cpus[0];
        for(unsigned i = 1; i < SMP::processorCount; i++){
            if(SMP::cpus[i]->runQueue->get_length() < cpu->runQueue->get_length()) {
                cpu = SMP::cpus[i];
            }

            if(!cpu->runQueue->get_length()){
                break;
            }
        }

        asm("sti");
        acquireLock(&cpu->runQueueLock);
        asm("cli");
        cpu->runQueue->add_back(thread);
        releaseLock(&cpu->runQueueLock);
        asm("sti");
    }

    void Initialize() {
        processes = new List<process_t*>();
        destroyedProcesses = new List<process_t*>();

        CPU* cpu = GetCPULocal();

        for(unsigned i = 0; i < SMP::processorCount; i++) {
            process_t* idleProcess = CreateProcess((void*)IdleProcess);
            strcpy(idleProcess->name, "IdleProcess");
            idleProcess->threads[0]->timeSliceDefault = 0;
            idleProcess->threads[0]->timeSlice = 0;
            SMP::cpus[i]->idleProcess = idleProcess;
        }

        for(unsigned i = 0; i < SMP::processorCount; i++) {
            SMP::cpus[i]->runQueue->clear();
            releaseLock(&SMP::cpus[i]->runQueueLock);
        }

        IDT::RegisterInterruptHandler(IPI_SCHEDULE, Schedule);

        auto kproc = CreateProcess((void*)KernelProcess);
        strcpy(kproc->name, "Kernel");

        cpu->currentThread = nullptr;
        schedulerReady = true;
        asm("sti");
        for(;;);
    }

    void ProcessStateThreadBlocker::WaitOn(process_t* process){
        waitingOn.add_back(process);
        process->blocking.add_back(this);
    }

    ProcessStateThreadBlocker::~ProcessStateThreadBlocker(){
        acquireLock(&lock);

        for(process_t* process : waitingOn){
            process->blocking.remove(this);
        }
        waitingOn.clear();

        releaseLock(&lock);
    }

	Handle& RegisterHandle(process_t* proc, FancyRefPtr<KernelObject> ko){
        Handle h;
        h.ko = ko;

        acquireLock(&proc->handleLock); // Prevent handle ID race conditions

        h.id = proc->handles.get_length() + 1; // Handle IDs start at 1
        Handle& ref = proc->handles.add_back(h);

        releaseLock(&proc->handleLock);

        return ref;
    }

	long FindHandle(process_t* proc, handle_id_t id, Handle** handle){
        if(id < 1 || id - 1 > static_cast<handle_id_t>(proc->handles.get_length())){
            return 1;
        }

        *handle = &proc->handles[id - 1]; // Handle IDs start at 1

        if(!(*handle)->ko.get()) return 2;

        return 0;
    }

	long DestroyHandle(process_t* proc, handle_id_t id){
        if(id < 1 || id - 1 > static_cast<handle_id_t>(proc->handles.get_length())){
            return 1;
        }

        proc->handles[id - 1] = {0, FancyRefPtr<KernelObject>(nullptr)}; // Handle IDs start at 1

        return 0;
    }

    process_t* FindProcessByPID(pid_t pid){
        for(process_t* proc : *processes){
            if(proc->pid == pid) return proc;
        }

        return nullptr;
    }

    pid_t GetNextProccessPID(pid_t pid){
        pid_t newPID = INT32_MAX;
        for(process_t* proc : *processes){
            if(proc->pid > pid && proc->pid < newPID){
                newPID = proc->pid;
            }
        }

        if(newPID == INT32_MAX){
            return 0;
        }

        return newPID;
    }

    process_t* InitializeProcessStructure(){
        // Create process structure
        process_t* proc = new process_t;

        proc->fileDescriptors.clear();
        proc->sharedMemory.clear();
        proc->children.clear();
        proc->blocking.clear();
        proc->threads.clear();

        proc->threads.add_back(new Thread);
        proc->threadCount = 1;

        memset(proc->threads[0], 0, sizeof(Thread));

        proc->creationTime = Timer::GetSystemUptimeStruct();

        // Reserve 3 file descriptors for stdin, out and err
        FsNode* nullDev = fs::ResolvePath("/dev/null");
        FsNode* logDev = fs::ResolvePath("/dev/kernellog");

        if(nullDev){
            proc->fileDescriptors.add_back(fs::Open(nullDev));
        } else {
            proc->fileDescriptors.add_back(nullptr);
            
            Log::Warning("Failed to find /dev/null");
        }
        
        if(logDev){
            proc->fileDescriptors.add_back(fs::Open(logDev));
            proc->fileDescriptors.add_back(fs::Open(logDev));
        } else {
            proc->fileDescriptors.add_back(nullptr);
            proc->fileDescriptors.add_back(nullptr);

            Log::Warning("Failed to find /dev/kernellog");
        }

        proc->parent = nullptr;
        proc->uid = 0;

        proc->addressSpace = Memory::CreateAddressSpace();
        proc->pid = nextPID++; // Set Process ID to the next availiable

        // Create structure for the main thread
        Thread* thread = proc->threads[0];

        thread->stack = 0;
        thread->priority = 1;
        thread->timeSliceDefault = 1;
        thread->timeSlice = thread->timeSliceDefault;
        thread->fsBase = 0;
        thread->state = ThreadStateRunning;

        thread->next = nullptr;
        thread->prev = nullptr;
        thread->parent = proc;

        RegisterContext* registers = &thread->registers;
        memset((uint8_t*)registers, 0, sizeof(RegisterContext));
        registers->rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
        registers->cs = KERNEL_CS; // Kernel CS
        registers->ss = KERNEL_SS; // Kernel SS

        thread->fxState = Memory::KernelAllocate4KPages(1); // Allocate Memory for the FPU/Extended Register State
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)thread->fxState, 1);
        memset(thread->fxState, 0, 4096);

        void* kernelStack = Memory::KernelAllocate4KPages(32); // Allocate Memory For Kernel Stack (128KB)
        for(int i = 0; i < 32; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),reinterpret_cast<uintptr_t>(kernelStack) + PAGE_SIZE_4K * i, 1);
        }
        memset(kernelStack, 0, PAGE_SIZE_4K * 32);
        thread->kernelStack = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(kernelStack) + PAGE_SIZE_4K * 32);

        ((fx_state_t*)thread->fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
        ((fx_state_t*)thread->fxState)->mxcsrMask = 0xffbf;
        ((fx_state_t*)thread->fxState)->fcw = 0x33f; // Default FPU Control Word State

        strcpy(proc->workingDir, "/"); // set root as default working dir
        strcpy(proc->name, "unknown");

        return proc;
    }

    void Yield(){
        CPU* cpu = GetCPULocal();
        
        if(cpu->currentThread) {
            cpu->currentThread->timeSlice = 0;
        }
        asm("int $0xFD"); // Send schedule IPI to self
    }

    process_t* CreateProcess(void* entry) {
        process_t* proc = InitializeProcessStructure();
        Thread* thread = proc->threads[0];

        void* stack = (void*)Memory::KernelAllocate4KPages(32);
        for(int i = 0; i < 32; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)stack + PAGE_SIZE_4K * i, 1);
        }
        memset(stack, 0, PAGE_SIZE_4K * 32);

        thread->stack = stack; // 128KB stack size
        thread->registers.rsp = (uintptr_t)thread->stack + PAGE_SIZE_4K * 32;
        thread->registers.rbp = (uintptr_t)thread->stack + PAGE_SIZE_4K * 32;
        thread->registers.rip = (uintptr_t)entry;

        InsertNewThreadIntoQueue(proc->threads[0]);

        processes->add_back(proc);

        return proc;
    }

    pid_t CreateChildThread(process_t* process, uintptr_t entry, uintptr_t stack, uint64_t cs, uint64_t ss){
        pid_t threadID = process->threadCount++;
        process->threads.add_back(new Thread);

        Thread& thread = *process->threads[threadID];

        thread.tid = threadID;

        thread.parent = process;
        thread.registers.rip = entry;
        thread.registers.rsp = stack;
        thread.registers.rbp = stack;
        thread.state = ThreadStateRunning;
        thread.stack = thread.stackLimit = reinterpret_cast<void*>(stack);

        thread.fxState = Memory::KernelAllocate4KPages(1); // Allocate Memory for the FPU/Extended Register State
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)thread.fxState, 1);
        memset(thread.fxState, 0, 1024);

        void* kernelStack = Memory::KernelAllocate4KPages(32); // Allocate Memory For Kernel Stack (128KB)
        for(int i = 0; i < 32; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),reinterpret_cast<uintptr_t>(kernelStack) + PAGE_SIZE_4K * i, 1);
        }
        memset(kernelStack, 0, PAGE_SIZE_4K * 32);
        thread.kernelStack = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(kernelStack) + PAGE_SIZE_4K * 32);
        
        RegisterContext* registers = &thread.registers;
        registers->rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
        thread.registers.cs = cs;
        thread.registers.ss = ss;
        thread.timeSliceDefault = THREAD_TIMESLICE_DEFAULT;
        thread.timeSlice = thread.timeSliceDefault;
        thread.priority = 4;
        
        ((fx_state_t*)thread.fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
        ((fx_state_t*)thread.fxState)->mxcsrMask = 0xffbf;
        ((fx_state_t*)thread.fxState)->fcw = 0x33f; // Default FPU Control Word State

        InsertNewThreadIntoQueue(&thread);

        return threadID;
    }

    void EndProcess(process_t* process){
        IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
            Log::Info("ending process: %s (%d)", process->name, process->pid);
        });

        while(process->children.get_length()){
            IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
                Log::Info("ending child: %s (%d)", process->children.get_front()->name, process->children.get_front()->pid);
            });
            EndProcess(process->children.get_front()); // Processes remove themselves from the list
        }
        
        CPU* cpu = GetCPULocal();
        List<Thread*> runningThreads;
        for(unsigned i = 0; i < process->threads.get_length(); i++){
            Thread* thread = process->threads[i];
            if(thread != cpu->currentThread && thread){
                if(thread->blocker && thread->state == ThreadStateBlocked){
                    thread->blocker->Interrupt(); // Stop the thread from blocking
                }
                thread->state = ThreadStateZombie;

                if(acquireTestLock(&thread->lock)){
                    runningThreads.add_back(thread); // Thread lock could not be acquired
                } else {
                    thread->state = ThreadStateBlocked; // We have acquired the lock so prevent the thread from getting scheduled
                    thread->timeSlice = thread->timeSliceDefault = 0;
                }
            }
        }

        asm("sti");
        while(runningThreads.get_length()){
            auto it = runningThreads.begin();
            while(it != runningThreads.end()){
                Thread* thread = *it;
                if(!acquireTestLock(&thread->lock)){ // Loop through all of the threads so we can acquire their locks
                    runningThreads.remove(*(it++));

                    thread->state = ThreadStateBlocked;
                    thread->timeSlice = thread->timeSliceDefault = 0;
                } else {
                    it++;
                }
            }

            Scheduler::GetCurrentThread()->Sleep(50000); // Sleep for 50 ms so we do not chew through CPU time in the event of a deadlock
        }

        while(process->blocking.get_length()){
            process->blocking.get_front()->Unblock(process); // It will handle list removal for us
        }

        IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
            Log::Info("removing threads from run queue...");
        });

        acquireLock(&cpu->runQueueLock);
        asm("cli");

        for(unsigned j = 0; j < cpu->runQueue->get_length(); j++){
            if(Thread* thread = cpu->runQueue->get_at(j); thread != cpu->currentThread && thread->parent == process) {
                cpu->runQueue->remove_at(j);
                Log::Error("removing thread");
            }
        }

        releaseLock(&cpu->runQueueLock);
        asm("sti");
        
        for(unsigned i = 0; i < SMP::processorCount; i++){
            if(i == cpu->id) continue; // Is current processor?

            CPU* other = SMP::cpus[i];
            asm("sti");
            acquireLock(&other->runQueueLock);
            asm("cli");

            while(other->currentThread && other->currentThread->parent == process); // The thread state should be blocked so just wait a few ticks
            
            for(unsigned j = 0; j < other->runQueue->get_length(); j++){
                Thread* thread = other->runQueue->get_at(j);

                assert(thread);

                if(thread->parent == process){
                    other->runQueue->remove(thread);

                    delete thread;
                }
            }
            
            releaseLock(&other->runQueueLock);
            asm("sti");

            if(other->currentThread == nullptr){
                APIC::Local::SendIPI(i, ICR_DSH_SELF, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);
            }
        }
        asm("sti");

        IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
            Log::Info("closing fds...");
        });

        for(fs_fd_t* fd : process->fileDescriptors){
            if(fd && fd->node)
                fd->node->Close();
            
            fd->node = nullptr;
            delete fd;
        }
        process->fileDescriptors.clear();
        
        IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
            Log::Info("closing handles...");
        });

        for(auto& h : process->handles){
            if(h.id && h.ko.get()){
                h.ko->Destroy();
            }
        }

        process->handles.clear();

        for(unsigned i = 0; i < process->sharedMemory.get_length(); i++){
            Memory::Free4KPages((void*)process->sharedMemory[i].base, process->sharedMemory[i].pageCount, process->addressSpace); // Make sure the physical memory does not get freed
        }

        //address_space_t* addressSpace = process->addressSpace;
        
        IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
            Log::Info("removing process...");
        });

        processes->remove(process);
        
        if(process->parent){
            process->parent->children.remove(process);
        }

        acquireLock(&destroyedProcessesLock);
        process->processLock.AcquireWrite();

        destroyedProcesses->add_back(process);

        releaseLock(&destroyedProcessesLock);

        bool isProcessToKill = cpu->currentThread->parent == process;
        if(!isProcessToKill){
            process->processLock.ReleaseWrite();
        }
        
        IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
            Log::Info("destroying address space...");
        });

        if(isProcessToKill){
            asm("cli");

            asm volatile("mov %%rax, %%cr3" :: "a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE));

            Memory::DestroyAddressSpace(process->addressSpace);

            process->processLock.ReleaseWrite();

            cpu->currentThread->state = ThreadStateDying;
            cpu->currentThread->timeSlice = 0;
            
            IF_DEBUG(debugLevelScheduler >= DebugLevelVerbose, {
                Log::Info("rescheduling...");
            });

            asm volatile("sti; int $0xFD"); // IPI_SCHEDULE
            assert(!"We should not be here");
        }
    }

    void Tick(RegisterContext* r){
        if(!schedulerReady) return;

        APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);

        Schedule(nullptr, r);
    }

    void Schedule(__attribute__((unused)) void* data, RegisterContext* r){
        CPU* cpu = GetCPULocal();

        if(cpu->currentThread) {
            cpu->currentThread->parent->activeTicks++;
            if(cpu->currentThread->timeSlice > 0) {
                cpu->currentThread->timeSlice--;
                return;
            }
        }

        while(__builtin_expect(acquireTestLock(&cpu->runQueueLock), 0)) {
            return;
        }

    
        if (__builtin_expect(cpu->runQueue->get_length() <= 0 || !cpu->currentThread, 0)){
            cpu->currentThread = cpu->idleProcess->threads[0];
        } else {
            if(__builtin_expect(cpu->currentThread->state == ThreadStateDying, 0)){
                cpu->runQueue->remove(cpu->currentThread);
                cpu->currentThread = cpu->idleProcess->threads[0];
            } else if(__builtin_expect(cpu->currentThread->parent != cpu->idleProcess, 1)){
                cpu->currentThread->timeSlice = cpu->currentThread->timeSliceDefault;

                asm volatile ("fxsave64 (%0)" :: "r"((uintptr_t)cpu->currentThread->fxState) : "memory");

                cpu->currentThread->registers = *r;

                cpu->currentThread = cpu->currentThread->next;
            } else {
                cpu->currentThread = cpu->runQueue->front;
            }
            
            if(cpu->currentThread->state == ThreadStateBlocked){
                Thread* first = cpu->currentThread;

                do {
                    cpu->currentThread = cpu->currentThread->next;
                } while(cpu->currentThread->state == ThreadStateBlocked && cpu->currentThread != first);

                if(cpu->currentThread->state == ThreadStateBlocked){
                    cpu->currentThread = cpu->idleProcess->threads[0];
                }
            }
        }

        releaseLock(&cpu->runQueueLock);

        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)cpu->currentThread->fxState) : "memory");

	    asm volatile ("wrmsr" :: "a"(cpu->currentThread->fsBase & 0xFFFFFFFF) /*Value low*/, "d"((cpu->currentThread->fsBase >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);

        TSS::SetKernelStack(&cpu->tss, (uintptr_t)cpu->currentThread->kernelStack);
	
        TaskSwitch(&cpu->currentThread->registers, cpu->currentThread->parent->addressSpace->pml4Phys);
    }

    process_t* CreateELFProcess(void* elf, int argc, char** argv, int envc, char** envp, const char* execPath) {
        if(!VerifyELF(elf)) {
            return nullptr;
        }

        // Create process structure
        process_t* proc = InitializeProcessStructure();

        Thread* thread = proc->threads[0];
        thread->registers.cs = USER_CS; // We want user mode so use user mode segments, make sure RPL is 3
        thread->registers.ss = USER_SS;
        thread->timeSliceDefault = THREAD_TIMESLICE_DEFAULT;
        thread->timeSlice = thread->timeSliceDefault;
        thread->priority = 4;

        Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),0,1,proc->addressSpace);

        elf_info_t elfInfo = LoadELFSegments(proc, elf, 0);
        
        thread->registers.rip = elfInfo.entry;
        
        if(elfInfo.linkerPath){
            //char* linkPath = elfInfo.linkerPath;
            uintptr_t linkerBaseAddress = 0x7FC0000000; // Linker base address

            FsNode* node = fs::ResolvePath("/lib/ld.so");
            assert(node);

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

            kfree(linkerElf);
        }

        char* tempArgv[argc];
        char* tempEnvp[envc];

        asm("cli");
        asm volatile("mov %%rax, %%cr3" :: "a"(proc->addressSpace->pml4Phys));
        void* _stack = (void*)Memory::Allocate4KPages(64, proc->addressSpace);
        for(int i = 0; i < 64; i++){
            Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)_stack + PAGE_SIZE_4K * i, 1, proc->addressSpace);
            proc->usedMemoryBlocks++;
        }
        memset(_stack, 0, PAGE_SIZE_4K * 64);

        thread->stack = _stack; // 256KB stack size
        thread->registers.rsp = (uintptr_t)thread->stack + PAGE_SIZE_4K * 64;
        thread->registers.rbp = (uintptr_t)thread->stack + PAGE_SIZE_4K * 64;

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

        char* execPathValue = nullptr;
        if(execPath){
            stackStr -= strlen(execPath) +  1;
            strcpy((char*)stackStr, execPath);

            execPathValue = stackStr;
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

        if(execPath && execPathValue){
            stack -= sizeof(auxv_t)/sizeof(*stack);
            *((auxv_t*)stack) = {.a_type = AT_EXECPATH, .a_val = (uint64_t)execPathValue}; // AT_EXECPATH
        }

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

        thread->registers.rsp = (uintptr_t) stack;
        thread->registers.rbp = (uintptr_t) stack;
        
        assert(!(thread->registers.rsp & 0xF));
        
        processes->add_back(proc);

        InsertNewThreadIntoQueue(proc->threads[0]);

        return proc;
    }
}
