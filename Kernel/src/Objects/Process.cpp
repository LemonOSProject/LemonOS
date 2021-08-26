#include <Objects/Process.h>

#include <ABI.h>
#include <APIC.h>
#include <Assert.h>
#include <CPU.h>
#include <ELF.h>
#include <IDT.h>
#include <SMP.h>
#include <Scheduler.h>
#include <String.h>
#include <Panic.h>

extern uint8_t signalTrampolineStart[];
extern uint8_t signalTrampolineEnd[];

void IdleProcess();

FancyRefPtr<Process> Process::CreateIdleProcess(const char* name){
    FancyRefPtr<Process> proc = new Process(Scheduler::GetNextPID(), name, "/", nullptr);

    proc->m_mainThread->registers.rip = reinterpret_cast<uintptr_t>(IdleProcess);
    proc->m_mainThread->timeSlice = 0;
    proc->m_mainThread->timeSliceDefault = 0;

    proc->m_mainThread->registers.rsp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);
    proc->m_mainThread->registers.rbp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);

    Scheduler::RegisterProcess(proc);
    return proc;
}

FancyRefPtr<Process> Process::CreateKernelProcess(void* entry, const char* name, Process* parent){
    FancyRefPtr<Process> proc = new Process(Scheduler::GetNextPID(), name, "/", parent);

    proc->m_mainThread->registers.rip = reinterpret_cast<uintptr_t>(entry);
    proc->m_mainThread->registers.rsp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);
    proc->m_mainThread->registers.rbp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);

    Scheduler::RegisterProcess(proc);
    return proc;
}

FancyRefPtr<Process> Process::CreateELFProcess(void* elf, const Vector<String>& argv, const Vector<String>& envp, const char* execPath, Process* parent){
    if (!VerifyELF(elf)) {
        return nullptr;
    }

    const char* name = "unknown";
    if(argv.size() >= 1){
        name = argv[0].c_str();
    }
    FancyRefPtr<Process> proc = new Process(Scheduler::GetNextPID(), name, "/", parent);

    Thread* thread = proc->m_mainThread.get();
    thread->registers.cs = USER_CS; // We want user mode so use user mode segments, make sure RPL is 3
    thread->registers.ss = USER_SS;
    thread->timeSliceDefault = THREAD_TIMESLICE_DEFAULT;
    thread->timeSlice = thread->timeSliceDefault;
    thread->priority = 4;

    elf_info_t elfInfo = LoadELFSegments(proc.get(), elf, 0);

    MappedRegion* stackRegion = proc->addressSpace->AllocateAnonymousVMObject(0x400000, 0, false); // 4MB max stacksize

    thread->stack = reinterpret_cast<void*>(stackRegion->base); // 4MB stack size
    thread->registers.rsp = (uintptr_t)thread->stack + 0x400000;
    thread->registers.rbp = (uintptr_t)thread->stack + 0x400000;

    // Force the first 12KB to be allocated
    stackRegion->vmObject->Hit(stackRegion->base, 0x400000 - 0x1000, proc->GetPageMap());
    stackRegion->vmObject->Hit(stackRegion->base, 0x400000 - 0x2000, proc->GetPageMap());
    stackRegion->vmObject->Hit(stackRegion->base, 0x400000 - 0x3000, proc->GetPageMap());

    thread->registers.rip = proc->LoadELF(&thread->registers.rsp, elfInfo, argv, envp, execPath);
    if (!thread->registers.rip) {
        proc->Die();
        delete proc->addressSpace;

        return nullptr;
    }

    assert(!(thread->registers.rsp & 0xF));

    // Reserve 3 file descriptors for stdin, out and err
    FsNode* nullDev = fs::ResolvePath("/dev/null");
    FsNode* logDev = fs::ResolvePath("/dev/kernellog");

    if (nullDev) {
        proc->m_fileDescriptors[0] = (fs::Open(nullDev)); // stdin
    } else {
        Log::Warning("Failed to find /dev/null");
    }

    if (logDev) {
        proc->m_fileDescriptors[1] = (fs::Open(logDev)); // stdout
        proc->m_fileDescriptors[2] = (fs::Open(logDev)); // stderr
    } else {
        Log::Warning("Failed to find /dev/kernellog");
    }

    proc->MapSignalTrampoline();

    Scheduler::RegisterProcess(proc);
    return proc;
}

Process::Process(pid_t pid, const char* _name, const char* _workingDir, Process* parent)
    : m_pid(pid), m_parent(parent) {
    if(_workingDir){
        strncpy(workingDir, _workingDir, PATH_MAX);
    } else {
        strcpy(workingDir, "/");
    }
    strncpy(name, _name, NAME_MAX);

    addressSpace = new AddressSpace(Memory::CreatePageMap());

    // Initialize signal handlers
    for (unsigned i = 0; i < SIGNAL_MAX; i++) {
        signalHandlers[i] = {
            .action = SignalHandler::ActionDefault,
            .flags = 0,
            .mask = 0,
            .userHandler = nullptr,
        };
    }

    creationTime = Timer::GetSystemUptimeStruct();

    m_mainThread = new Thread(this, m_nextThreadID++);
    m_threads.add_back(m_mainThread);

    assert(m_mainThread->parent == this);

    m_fileDescriptors.add_back(nullptr); // stdin
    m_fileDescriptors.add_back(nullptr); // stdout
    m_fileDescriptors.add_back(nullptr); // stderr
}

uintptr_t Process::LoadELF(uintptr_t* stackPointer, elf_info_t elfInfo, const Vector<String>& argv, const Vector<String>& envp, const char* execPath) {
    uintptr_t rip = elfInfo.entry;
    if (elfInfo.linkerPath) {
        // char* linkPath = elfInfo.linkerPath;
        uintptr_t linkerBaseAddress = 0x7FC0000000; // Linker base address

        FsNode* node = fs::ResolvePath("/lib/ld.so");
        if (!node) {
            KernelPanic("Failed to load dynamic linker!");
        }

        void* linkerElf = kmalloc(node->size);

        fs::Read(node, 0, node->size, (uint8_t*)linkerElf); // Load Dynamic Linker

        if (!VerifyELF(linkerElf)) {
            Log::Warning("Invalid Dynamic Linker ELF");
            return 0;
        }

        elf_info_t linkerELFInfo = LoadELFSegments(this, linkerElf, linkerBaseAddress);

        rip = linkerELFInfo.entry;

        kfree(linkerElf);
    }

    char* tempArgv[argv.size()];
    char* tempEnvp[envp.size()];

    asm("cli");
    asm volatile("mov %%rax, %%cr3" ::"a"(this->GetPageMap()->pml4Phys));

    // ABI Stuff
    uint64_t* stack = (uint64_t*)(*stackPointer);

    char* stackStr = (char*)stack;
    for (int i = 0; i < argv.size(); i++) {
        stackStr -= argv[i].Length() + 1;
        tempArgv[i] = stackStr;
        strcpy((char*)stackStr, argv[i].c_str());
    }

    for (int i = 0; i < envp.size(); i++) {
        stackStr -= envp[i].Length() + 1;
        tempEnvp[i] = stackStr;
        strcpy((char*)stackStr, envp[i].c_str());
    }

    char* execPathValue = nullptr;
    if (execPath) {
        stackStr -= strlen(execPath) + 1;
        strcpy((char*)stackStr, execPath);

        execPathValue = stackStr;
    }

    stackStr -= (uintptr_t)stackStr & 0xf; // align the stack

    stack = (uint64_t*)stackStr;

    stack -= ((argv.size() + envp.size()) % 2); // If argc + envc is odd then the stack will be misaligned

    stack--;
    *stack = 0; // AT_NULL

    stack -= sizeof(auxv_t) / sizeof(*stack);
    *((auxv_t*)stack) = {.a_type = AT_PHDR, .a_val = elfInfo.pHdrSegment}; // AT_PHDR

    stack -= sizeof(auxv_t) / sizeof(*stack);
    *((auxv_t*)stack) = {.a_type = AT_PHENT, .a_val = elfInfo.phEntrySize}; // AT_PHENT

    stack -= sizeof(auxv_t) / sizeof(*stack);
    *((auxv_t*)stack) = {.a_type = AT_PHNUM, .a_val = elfInfo.phNum}; // AT_PHNUM

    stack -= sizeof(auxv_t) / sizeof(*stack);
    *((auxv_t*)stack) = {.a_type = AT_ENTRY, .a_val = elfInfo.entry}; // AT_ENTRY

    if (execPath && execPathValue) {
        stack -= sizeof(auxv_t) / sizeof(*stack);
        *((auxv_t*)stack) = {.a_type = AT_EXECPATH, .a_val = (uint64_t)execPathValue}; // AT_EXECPATH
    }

    stack--;
    *stack = 0; // null

    stack -= envp.size();
    for (int i = 0; i < envp.size(); i++) {
        *(stack + i) = (uint64_t)tempEnvp[i];
    }

    stack--;
    *stack = 0; // null

    stack -= argv.size();
    for (int i = 0; i < argv.size(); i++) {
        *(stack + i) = (uint64_t)tempArgv[i];
    }

    stack--;
    *stack = argv.size(); // argc

    asm volatile("mov %%rax, %%cr3" ::"a"(Scheduler::GetCurrentProcess()->GetPageMap()->pml4Phys));
    asm("sti");

    *stackPointer = (uintptr_t)stack;
    return rip;
}

Process::~Process() {
    ScopedSpinLock lock(m_processLock);
    assert(m_state == Process_Dead);
    assert(!m_parent);

    if (addressSpace) {
        delete addressSpace;
    }
}

void Process::Destroy() {
    if (m_state != Process_Dead) {
        // Use SIGKILL as the process will kill itself
        KernelObjectWatcher watcher;
        Watch(watcher, 0);

        m_mainThread->Signal(SIGKILL);
        bool interrupted = watcher.Wait(); // Wait for the process to die

        assert(!interrupted);
        assert(m_state == Process_Dead);
    }

    // Remove from parent completely
    if (m_parent) {
        ScopedSpinLock acq(m_parent->m_processLock);
        for (auto it = m_parent->m_children.begin(); it != m_parent->m_children.end(); it++) {
            if (it->get() == this) {
                m_parent->m_children.remove(it); // Remove ourselves
                break;
            }
        }
        m_parent = nullptr;
    }
}

void Process::Die() {
    CPU* cpu = GetCPULocal();

    m_state = Process_Dying;
    Log::Debug(debugLevelScheduler, DebugLevelNormal, "Killing Process %s (PID %d)", name, m_pid);

    // Ensure the current thread's lock is acquired
    assert(acquireTestLock(&Scheduler::GetCurrentThread()->lock));

    acquireLock(&m_processLock);
    List<FancyRefPtr<Thread>> runningThreads;

    for (auto& thread : m_threads) {
        asm("cli");
        if (thread != cpu->currentThread && thread) {
            // TODO: Race condition?
            if (thread->blocker && thread->state == ThreadStateBlocked) {
                thread->blocker->Interrupt(); // Stop the thread from blocking
            }
            thread->state = ThreadStateZombie;

            if (acquireTestLock(&thread->lock)) {
                asm("sti");
                runningThreads.add_back(thread); // Thread lock could not be acquired
                asm("cli");
            } else {
                thread->state =
                    ThreadStateBlocked; // We have acquired the lock so prevent the thread from getting scheduled
                thread->timeSlice = thread->timeSliceDefault = 0;
            }
        }
    }

    while (m_children.get_length()) {
        FancyRefPtr<Process> child = m_children.get_front();
        if (child->State() == Process_Running) {
            child->Die(); // Kill it, burn it with fire
        } else if (child->State() == Process_Dying) {
            KernelObjectWatcher w;
            child->Watch(w, 0);

            bool wasInterrupted = w.Wait(); // Wait for the process to die
            assert(!wasInterrupted);
        }

        child->m_parent = nullptr;
        m_children.remove(child); // Remove from child list
    }

    // Ensure we release this at least momentarily in case threads are waiting on said lock
    releaseLock(&m_processLock);

    asm("sti");
    while (runningThreads.get_length()) {
        auto it = runningThreads.begin();
        while (it != runningThreads.end()) {
            FancyRefPtr<Thread> thread = *it;
            if (!acquireTestLock(&thread->lock)) { // Loop through all of the threads so we can acquire their locks
                runningThreads.remove(*(it++));

                thread->state = ThreadStateBlocked;
                thread->timeSlice = thread->timeSliceDefault = 0;
            } else {
                it++;
            }
        }

        Scheduler::GetCurrentThread()->Sleep(50000); // Sleep for 50 ms so we do not chew through CPU time
    }

    assert(!runningThreads.get_length());

    acquireLock(&m_processLock);
    acquireLock(&cpu->runQueueLock);
    asm("cli");

    for (unsigned j = 0; j < cpu->runQueue->get_length(); j++) {
        if (Thread* thread = cpu->runQueue->get_at(j); thread != cpu->currentThread && thread->parent == this) {
            cpu->runQueue->remove_at(j);
        }
    }

    releaseLock(&cpu->runQueueLock);
    asm("sti");

    for (unsigned i = 0; i < SMP::processorCount; i++) {
        if (i == cpu->id)
            continue; // Is current processor?

        CPU* other = SMP::cpus[i];
        asm("sti");
        acquireLock(&other->runQueueLock);
        asm("cli");

        assert(!(other->currentThread && other->currentThread->parent == this)); // The thread state should be blocked

        for (unsigned j = 0; j < other->runQueue->get_length(); j++) {
            Thread* thread = other->runQueue->get_at(j);

            assert(thread);

            if (thread->parent == this) {
                other->runQueue->remove(thread);
            }
        }

        releaseLock(&other->runQueueLock);
        asm("sti");

        if (other->currentThread == nullptr) {
            APIC::Local::SendIPI(i, ICR_DSH_SELF, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);
        }
    }
    asm("sti");

    // All threads have ceased, set state to dead
    m_state = Process_Dead;

    Log::Debug(debugLevelScheduler, DebugLevelVerbose, "[%d] Closing fds...", m_pid);
    for (auto& fd : m_fileDescriptors) {
        // Deferences the file descriptor
        fd = nullptr;
    }
    m_fileDescriptors.clear();

    Log::Debug(debugLevelScheduler, DebugLevelVerbose, "[%d] Closing handles...", m_pid);
    for (auto& handle : m_handles) {
        if(handle.ko.get()){
            handle.ko->Destroy();
        }
        // Deferences the handle
        handle.ko = nullptr;
    }
    m_handles.clear();

    Log::Debug(debugLevelScheduler, DebugLevelVerbose, "[%d] Signaling watchers...", m_pid);
    for(auto* watcher : m_watching){
        watcher->Signal();

    }
    m_watching.clear();

    if(m_parent && (m_parent->State() == Process_Running)){
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "[%d] Sending SIGCHILD to %s...", m_pid, m_parent->name);
        m_parent->GetMainThread()->Signal(SIGCHLD);
    }

    // Add to destroyed processes so the reaper thread can safely destroy any last resources
    Log::Debug(debugLevelScheduler, DebugLevelVerbose, "[%d] Marking process for destruction...", m_pid);
    Scheduler::MarkProcessForDestruction(this);

    bool isDyingProcess = (cpu->currentThread->parent == this);
    if(isDyingProcess){
        asm("cli");

        asm volatile("mov %%rax, %%cr3" ::"a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE));

        cpu->currentThread->state = ThreadStateDying;
        cpu->currentThread->timeSlice = 0;

        releaseLock(&m_processLock);

        asm volatile("sti; int $0xFD"); // IPI_SCHEDULE
        assert(!"We should not be here");
    } else {
        releaseLock(&m_processLock);
    }
}

void Process::Start(){
    ScopedSpinLock acq(m_processLock);
    assert(!m_started);

    Scheduler::InsertNewThreadIntoQueue(m_mainThread.get());
    m_started = true;
}

void Process::Watch(KernelObjectWatcher& watcher, int events) {
    ScopedSpinLock acq(m_watchingLock);

    if (m_state == Process_Dead) {
        watcher.Signal();
        return; // Process is already dead
    }

    m_watching.add_back(&watcher);
}

void Process::Unwatch(KernelObjectWatcher& watcher) {
    ScopedSpinLock acq(m_watchingLock);

    if (m_state == Process_Dead) {
        return; // Should already be removed from watching
    }

    m_watching.remove(&watcher);
}

FancyRefPtr<Process> Process::Fork() {
    ScopedSpinLock lock(m_processLock);

    FancyRefPtr<Process> newProcess = new Process(Scheduler::GetNextPID(), name, workingDir, this);
    delete newProcess->addressSpace; // TODO: Do not create address space in first place
    newProcess->addressSpace = addressSpace->Fork();

    newProcess->euid = euid;
    newProcess->uid = uid;
    newProcess->euid = egid;
    newProcess->gid = gid;

    newProcess->m_fileDescriptors.resize(m_fileDescriptors.size());
    for(unsigned i = 0; i < m_fileDescriptors.size(); i++){
        UNIXFileDescriptor* source = m_fileDescriptors[i].get();
        if(!source || !source->node){
            continue;
        }

        UNIXFileDescriptor* dest = fs::Open(source->node);

        dest->mode = source->mode;
        dest->pos = source->pos;

        newProcess->m_fileDescriptors[i] = dest;
    }

    m_children.add_back(newProcess);
    Scheduler::RegisterProcess(newProcess);
    return newProcess;
}

pid_t Process::CreateChildThread(uintptr_t entry, uintptr_t stack, uint64_t cs, uint64_t ss){
    pid_t threadID = m_nextThreadID++;
    Thread& thread = *m_threads.add_back(new Thread(this, threadID));

    thread.registers.rip = entry;
    thread.registers.rsp = stack;
    thread.registers.rbp = stack;
    thread.state = ThreadStateRunning;
    thread.stack = thread.stackLimit = reinterpret_cast<void*>(stack);

    RegisterContext* registers = &thread.registers;
    registers->rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
    thread.registers.cs = cs;
    thread.registers.ss = ss;
    thread.timeSliceDefault = THREAD_TIMESLICE_DEFAULT;
    thread.timeSlice = thread.timeSliceDefault;
    thread.priority = 4;

    Scheduler::InsertNewThreadIntoQueue(&thread);
    return threadID;
}

int Process::AllocateFileDescriptor(FancyRefPtr<UNIXFileDescriptor> fd){
    ScopedSpinLock lockFDs(m_fileDescriptorLock);

    int i = 0;
    for(; i < static_cast<int>(m_fileDescriptors.get_length()); i++){
        if(m_fileDescriptors[i] == nullptr){
            m_fileDescriptors[i] = std::move(fd);
            return i;
        }
    }

    m_fileDescriptors.add_back(std::move(fd));
    return i;
}

FancyRefPtr<Thread> Process::GetThreadFromTID_Unlocked(pid_t tid) {
    for (const FancyRefPtr<Thread>& t : m_threads) {
        if (t->tid == tid) {
            return t;
        }
    }

    return nullptr;
}

void Process::MapSignalTrampoline(){
    // Allocate space for both a siginfo struct and the signal trampoline
    m_signalTrampoline = addressSpace->AllocateAnonymousVMObject(
        ((signalTrampolineEnd - signalTrampolineStart) + PAGE_SIZE_4K - 1) &
            ~static_cast<unsigned>(PAGE_SIZE_4K - 1),
        0, false);
    reinterpret_cast<PhysicalVMObject*>(m_signalTrampoline->vmObject.get())
        ->ForceAllocate(); // Forcibly allocate all blocks
    m_signalTrampoline->vmObject->MapAllocatedBlocks(m_signalTrampoline->Base(), GetPageMap());

    // Copy signal trampoline code into process
    asm volatile("cli; mov %%rax, %%cr3" ::"a"(GetPageMap()->pml4Phys));
    memcpy(reinterpret_cast<void*>(m_signalTrampoline->Base()), signalTrampolineStart,
           signalTrampolineEnd - signalTrampolineStart);
    asm volatile("mov %%rax, %%cr3; sti" ::"a"(Scheduler::GetCurrentProcess()->GetPageMap()->pml4Phys));
}