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
#include <timer.h>

#define INITIAL_HANDLE_TABLE_SIZE 0xFFFF

extern "C" [[noreturn]] void TaskSwitch(regs64_t* r, uint64_t pml4);

extern "C"
void IdleProc();

void KernelProcess();

namespace Scheduler{
    int schedulerLock = 0;
    bool schedulerReady = false;

    List<process_t*>* processes;
    unsigned processTableSize = 512;
    uint64_t nextPID = 1;

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
        for(unsigned i = 1; i < SMP::processorCount; i++){
            if(SMP::cpus[i]->runQueue->get_length() < cpu->runQueue->get_length()) {
                cpu = SMP::cpus[i];
            }

            if(!cpu->runQueue->get_length()){
                break;
            }
        }

        //Log::Info("Inserting thread into run queue of CPU %d", cpu->id);

        asm("sti");
        acquireLock(&cpu->runQueueLock);
        asm("cli");
        cpu->runQueue->add_back(thread);
        releaseLock(&cpu->runQueueLock);
        asm("sti");
    }

    void Initialize() {
        memset(handles, 0, handleTableSize);

        processes = new List<process_t*>();

        CPU* cpu = GetCPULocal();

        for(unsigned i = 0; i < SMP::processorCount; i++) {
            SMP::cpus[i]->idleProcess = CreateProcess((void*)IdleProc);
            strcpy(SMP::cpus[i]->idleProcess->name, "IdleProcess");
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
        for(process_t* proc : *processes){
            if(proc->pid == pid) return proc;
        }

        return nullptr;
    }

    uint64_t GetNextProccessPID(uint64_t pid){
        uint64_t newPID = UINT64_MAX;
        for(process_t* proc : *processes){
            if(proc->pid > pid && proc->pid < newPID){
                newPID = proc->pid;
            }
        }

        if(newPID == UINT64_MAX){
            return 0;
        }

        return newPID;
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
        proc->blocking.clear();

        proc->threads = new thread_t;
        proc->threadCount = 1;

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
        thread_t* thread = proc->threads;

        thread->stack = 0;
        thread->priority = 1;
        thread->timeSliceDefault = 1;
        thread->timeSlice = thread->timeSliceDefault;
        thread->fsBase = 0;
        thread->state = ThreadStateRunning;

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

        void* kernelStack = (void*)Memory::KernelAllocate4KPages(32); // Allocate Memory For Kernel Stack (128KB)
        for(int i = 0; i < 32; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)kernelStack + PAGE_SIZE_4K * i, 1);
        }

        thread->kernelStack = kernelStack + PAGE_SIZE_4K * 32;

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
        thread_t* thread = &proc->threads[0];

        void* stack = (void*)Memory::KernelAllocate4KPages(32);//, proc->addressSpace);
        for(int i = 0; i < 32; i++){
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)stack + PAGE_SIZE_4K * i, 1);//, proc->addressSpace);
        }

        thread->stack = stack; // 128KB stack size
        thread->registers.rsp = (uintptr_t)thread->stack + PAGE_SIZE_4K * 32;
        thread->registers.rbp = (uintptr_t)thread->stack + PAGE_SIZE_4K * 32;
        thread->registers.rip = (uintptr_t)entry;

        InsertNewThreadIntoQueue(&proc->threads[0]);

        processes->add_back(proc);

        return proc;
    }

    void EndProcess(process_t* process){
        asm("sti");
        for(unsigned i = 0; i < process->children.get_length(); i++){
            EndProcess(process->children.get_at(i));
        }
        
        CPU* cpu = GetCPULocal();
        
        for(unsigned i = 0; i < process->threadCount; i++){
            if(&process->threads[i] != cpu->currentThread){
                acquireLock(&process->threads[i].lock); // Make sure we acquire a lock on all threads to ensure that they are not in a syscall and are not retaining a lock
            }
        }
        
        if(process->parent){
            process->parent->children.remove(process);
        }
        
        processes->remove(process);

        for(thread_t* t : process->blocking){
            UnblockThread(t);
        }

        for(unsigned i = 0; i < process->threadCount; i++){
            thread_t* thread = &process->threads[i];

            for(List<thread_t*>* queue : thread->waiting){
                queue->remove(thread);
            }
            
            process->threads[i].waiting.clear();

            thread->state = ThreadStateBlocked;
            thread->timeSlice = thread->timeSliceDefault = 0;
        }

        for(unsigned i = 0; i < process->fileDescriptors.get_length(); i++){
            if(process->fileDescriptors[i]){
                fs::Close(process->fileDescriptors[i]);
            }
        }

        acquireLock(&cpu->runQueueLock);
        asm("cli");

        for(unsigned j = 0; j < cpu->runQueue->get_length(); j++){
            if(cpu->runQueue->get_at(j)->parent == process) cpu->runQueue->remove_at(j);
        }

        process->fileDescriptors.clear();
        
        for(unsigned i = 0; i < SMP::processorCount; i++){
            if(i == cpu->id) continue; // Is current processor?

            if(SMP::cpus[i]->currentThread->parent == process){
                SMP::cpus[i]->currentThread = nullptr;
            }

            asm("sti");
            //acquireLock(&SMP::cpus[i]->runQueueLock);
            asm("cli");
            
            for(unsigned j = 0; j < SMP::cpus[i]->runQueue->get_length(); j++){
                thread_t* thread = SMP::cpus[i]->runQueue->get_at(j);

                assert(thread);

                if(thread->parent == process){
                    SMP::cpus[i]->runQueue->remove(thread);
                }
            }
            
            //releaseLock(&SMP::cpus[i]->runQueueLock);

            if(SMP::cpus[i]->currentThread == nullptr){
                APIC::Local::SendIPI(i, 0, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);
            }
        }

        if(cpu->currentThread->parent == process){
            asm volatile("mov %%rax, %%cr3" :: "a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE)); // If we are using the PML4 of the current process switch to the kernel's
        }

        for(unsigned i = 0; i < process->sharedMemory.get_length(); i++){
            Memory::Free4KPages((void*)process->sharedMemory[i].base, process->sharedMemory[i].pageCount, process->addressSpace); // Make sure the physical memory does not get freed
        }

        Memory::DestroyAddressSpace(process->addressSpace);

        for(unsigned i = 0; i < process->threadCount; i++){
            process->threads[i].waiting.~List();
        }

        if(cpu->currentThread->parent == process){
            cpu->currentThread = nullptr; // Force reschedule
            kfree(process);
            releaseLock(&cpu->runQueueLock);
            asm("sti");

            Schedule(nullptr);
            for(;;) {
                asm("hlt");
                Schedule(nullptr);
            }
        }

        releaseLock(&cpu->runQueueLock);
        kfree(process);
        asm("sti");
    }

	void BlockCurrentThread(List<thread_t*>& list, lock_t& lock){
        CPU* cpu = GetCPULocal();

        acquireLock(&lock);
        acquireLock(&cpu->runQueueLock);
        list.add_back(cpu->currentThread);
        cpu->currentThread->state = ThreadStateBlocked;
        releaseLock(&lock);
        releaseLock(&cpu->runQueueLock);

        Yield();
    }

	void BlockCurrentThread(ThreadBlocker& blocker, lock_t& lock){
        CPU* cpu = GetCPULocal();

        acquireLock(&lock);
        acquireLock(&cpu->runQueueLock);
        acquireLock(&cpu->currentThread->stateLock);
        blocker.Block(cpu->currentThread);
        cpu->currentThread->state = ThreadStateBlocked;
        releaseLock(&cpu->currentThread->stateLock);
        releaseLock(&lock);
        releaseLock(&cpu->runQueueLock);

        Yield();
    }
    
	void UnblockThread(thread_t* thread){
        acquireLock(&thread->stateLock);
        thread->state = ThreadStateRunning;
        releaseLock(&thread->stateLock);

        /*for(List<thread_t*>* l : thread->waiting){
            l->remove(thread);
        }*/
    }

    void Tick(regs64_t* r){
        if(!schedulerReady) return;

        APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);

        Schedule(r);
    }

    void Schedule(regs64_t* r){
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

        if (__builtin_expect(cpu->runQueue->get_length() <= 0 || !cpu->runQueue->front, 0)){
            cpu->currentThread = &cpu->idleProcess->threads[0];
        } else if(__builtin_expect(cpu->currentThread && cpu->currentThread->parent != cpu->idleProcess, 1)){
            cpu->currentThread->timeSlice = cpu->currentThread->timeSliceDefault;

            asm volatile ("fxsave64 (%0)" :: "r"((uintptr_t)cpu->currentThread->fxState) : "memory");

            cpu->currentThread->registers = *r;

            cpu->currentThread = cpu->currentThread->next;
        } else {
            cpu->currentThread = cpu->runQueue->front;
        }
            
        if(cpu->currentThread->state == ThreadStateBlocked){
            thread_t* first = cpu->currentThread;

            do {
                cpu->currentThread = cpu->currentThread->next;
            } while(cpu->currentThread->state == ThreadStateBlocked && cpu->currentThread != first);

            if(cpu->currentThread->state == ThreadStateBlocked){
                cpu->currentThread = &cpu->idleProcess->threads[0];
            }
        }

        releaseLock(&cpu->runQueueLock);
        asm volatile ("fxrstor64 (%0)" :: "r"((uintptr_t)cpu->currentThread->fxState) : "memory");

	    asm volatile ("wrmsr" :: "a"(cpu->currentThread->fsBase & 0xFFFFFFFF) /*Value low*/, "d"((cpu->currentThread->fsBase >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);
        
        TSS::SetKernelStack(&cpu->tss, (uintptr_t)cpu->currentThread->kernelStack);

        TaskSwitch(&cpu->currentThread->registers, cpu->currentThread->parent->addressSpace->pml4Phys);
    }

    process_t* CreateELFProcess(void* elf, int argc, char** argv, int envc, char** envp) {
        if(!VerifyELF(elf)) return nullptr;

        // Create process structure
        process_t* proc = InitializeProcessStructure();

        thread_t* thread = &proc->threads[0];
        thread->registers.cs = 0x1B; // We want user mode so use user mode segments, make sure RPL is 3
        thread->registers.ss = 0x23;
        thread->timeSliceDefault = 6;
        thread->timeSlice = thread->timeSliceDefault;
        thread->priority = 4;

        Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),0,1,proc->addressSpace);

        elf_info_t elfInfo = LoadELFSegments(proc, elf, 0);
        
        thread->registers.rip = elfInfo.entry;
        
        if(elfInfo.linkerPath){
            //char* linkPath = elfInfo.linkerPath;
            uintptr_t linkerBaseAddress = 0x7FC0000000; // Linker base address

            FsNode* node = fs::ResolvePath("/initrd/ld.so");

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

        char** tempArgv = (char**)kmalloc(argc * sizeof(char*));
        char** tempEnvp = (char**)kmalloc((envc) * sizeof(char*));

        asm("cli");
        asm volatile("mov %%rax, %%cr3" :: "a"(proc->addressSpace->pml4Phys));
        void* _stack = (void*)Memory::Allocate4KPages(64, proc->addressSpace);
        for(int i = 0; i < 64; i++){
            Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)_stack + PAGE_SIZE_4K * i, 1, proc->addressSpace);
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
        
        processes->add_back(proc);

        InsertNewThreadIntoQueue(&proc->threads[0]);

        return proc;
    }
}
