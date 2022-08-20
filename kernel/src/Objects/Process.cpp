#include <Objects/Process.h>

#include <ABI.h>
#include <APIC.h>
#include <Assert.h>
#include <CPU.h>
#include <ELF.h>
#include <IDT.h>
#include <Panic.h>
#include <SMP.h>
#include <Scheduler.h>
#include <String.h>
#include <hiraku.h>

#include <abi/peb.h>

extern uint8_t signalTrampolineStart[];
extern uint8_t signalTrampolineEnd[];

extern uint8_t _user_shared_data[];
extern uint8_t _hiraku[];
extern uint8_t _hiraku_end[];
extern uint8_t _user_shared_data_end[];

// the user_shared_data section is aligned to page size.
class UserSharedData : public VMObject {
public:
    UserSharedData()
        : VMObject(PAGE_COUNT_4K(_user_shared_data_end - _user_shared_data) << PAGE_SHIFT_4K, false, true) {}

    void MapAllocatedBlocks(uintptr_t base, PageMap* pMap) {
        Memory::MapVirtualMemory4K(((uintptr_t)_user_shared_data - KERNEL_VIRTUAL_BASE), base, size >> PAGE_SHIFT_4K,
                                   PAGE_USER | PAGE_PRESENT, pMap);
    }

    [[noreturn]] VMObject* Clone() { assert(!"user_shared_data VMO cannot be cloned!"); }
};
lock_t userSharedDataLock = 0;
FancyRefPtr<UserSharedData> userSharedDataVMO = nullptr;

void IdleProcess();

FancyRefPtr<Process> Process::CreateIdleProcess(const char* name) {
    FancyRefPtr<Process> proc = new Process(Scheduler::GetNextPID(), name, "/", nullptr);

    proc->m_mainThread->registers.rip = reinterpret_cast<uintptr_t>(IdleProcess);
    proc->m_mainThread->timeSlice = 0;
    proc->m_mainThread->timeSliceDefault = 0;

    proc->m_mainThread->registers.rsp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);
    proc->m_mainThread->registers.rbp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);

    proc->m_isIdleProcess = true;

    Scheduler::RegisterProcess(proc);
    return proc;
}

FancyRefPtr<Process> Process::CreateKernelProcess(void* entry, const char* name, Process* parent) {
    FancyRefPtr<Process> proc = new Process(Scheduler::GetNextPID(), name, "/", parent);

    proc->m_mainThread->registers.rip = reinterpret_cast<uintptr_t>(entry);
    proc->m_mainThread->registers.rsp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);
    proc->m_mainThread->registers.rbp = reinterpret_cast<uintptr_t>(proc->m_mainThread->kernelStack);

    Scheduler::RegisterProcess(proc);
    return proc;
}

FancyRefPtr<Process> Process::CreateELFProcess(void* elf, const Vector<String>& argv, const Vector<String>& envp,
                                               const char* execPath, Process* parent) {
    if (!VerifyELF(elf)) {
        return nullptr;
    }

    const char* name = "unknown";
    if (argv.size() >= 1) {
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

    MappedRegion* stackRegion =
        proc->addressSpace->AllocateAnonymousVMObject(0x400000, 0x7000000000, false); // 4MB max stacksize

    thread->stack = reinterpret_cast<void*>(stackRegion->base); // 4MB stack size
    thread->registers.rsp = (uintptr_t)thread->stack + 0x400000;
    thread->registers.rbp = (uintptr_t)thread->stack + 0x400000;

    // Force the first 12KB to be allocated
    stackRegion->vmObject->Hit(stackRegion->base, 0x400000 - 0x1000, proc->GetPageMap());
    stackRegion->vmObject->Hit(stackRegion->base, 0x400000 - 0x2000, proc->GetPageMap());
    stackRegion->vmObject->Hit(stackRegion->base, 0x400000 - 0x3000, proc->GetPageMap());

    proc->MapUserSharedData();

    proc->pebRegion = proc->addressSpace->AllocateAnonymousVMObject(
        PAGE_COUNT_4K(sizeof(ProcessEnvironmentBlock)) << PAGE_SHIFT_4K, 0x7000800000, false);
    proc->pebRegion->vmObject->Hit(proc->pebRegion->base, 0, proc->GetPageMap());

    thread->gsBase = proc->pebRegion->Base();
    Log::Info("pebbase %x", thread->gsBase);

    thread->registers.rip = proc->LoadELF(&thread->registers.rsp, elfInfo, argv, envp, execPath);
    if (!thread->registers.rip) {
        proc->Die();
        delete proc->addressSpace;
        proc->addressSpace = nullptr;

        return nullptr;
    }

    assert(!(thread->registers.rsp & 0xF));

    // Reserve 3 file descriptors for stdin, out and err
    FsNode* nullDev = fs::ResolvePath("/dev/null");
    FsNode* logDev = fs::ResolvePath("/dev/kernellog");

    if (nullDev) {
        proc->m_handles[0] = MakeHandle(0, fs::Open(nullDev).Value()); // stdin
    } else {
        Log::Warning("Failed to find /dev/null");
    }

    if (logDev) {
        proc->m_handles[1] = MakeHandle(1, fs::Open(logDev).Value()); // stdout
        proc->m_handles[2] = MakeHandle(2, fs::Open(logDev).Value()); // stderr
    } else {
        Log::Warning("Failed to find /dev/kernellog");
    }

    Scheduler::RegisterProcess(proc);
    return proc;
}

Process::Process(pid_t pid, const char* _name, const char* _workingDir, Process* parent)
    : m_pid(pid), m_parent(parent) {
    if (_workingDir) {
        strncpy(workingDirPath, _workingDir, PATH_MAX);
    } else {
        strcpy(workingDirPath, "/");
    }

    FsNode* wdNode = fs::ResolvePath(workingDirPath);
    assert(wdNode);
    assert(wdNode->IsDirectory());

    if (auto openFile = wdNode->Open(0); !openFile.HasError()) {
        workingDir = std::move(openFile.Value());
    } else {
        assert(!openFile.HasError());
    }

    strncpy(name, _name, NAME_MAX);

    addressSpace = new AddressSpace(Memory::CreatePageMap());

    // Initialize signal handlers
    for (unsigned i = 0; i < SIGNAL_MAX; i++) {
        signalHandlers[i] = {
            .action = SignalHandler::HandlerAction::Default,
            .flags = 0,
            .mask = 0,
            .userHandler = nullptr,
        };
    }

    creationTime = Timer::GetSystemUptimeStruct();

    m_mainThread = new Thread(this, m_nextThreadID++);
    m_threads.add_back(m_mainThread);

    assert(m_mainThread->parent == this);

    m_handles.add_back(HANDLE_NULL); // stdin
    m_handles.add_back(HANDLE_NULL); // stdout
    m_handles.add_back(HANDLE_NULL); // stderr
}

uintptr_t Process::LoadELF(uintptr_t* stackPointer, elf_info_t elfInfo, const Vector<String>& argv,
                           const Vector<String>& envp, const char* execPath) {
    uintptr_t rip = elfInfo.entry;
    uintptr_t linkerBaseAddress = 0x7FC0000000; // Linker base address
    elf_info_t linkerELFInfo;

    if (elfInfo.linkerPath) {
        // char* linkPath = elfInfo.linkerPath;

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

        linkerELFInfo = LoadELFSegments(this, linkerElf, linkerBaseAddress);
        rip = linkerELFInfo.entry;

        kfree(linkerElf);
    }

    // char* tempArgv[argv.size()];
    // char* tempEnvp[envp.size()];

    // ABI Stuff
    uint64_t* stack = (uint64_t*)(*stackPointer);

    asm("cli");
    asm volatile("mov %%rax, %%cr3" ::"a"(this->GetPageMap()->pml4Phys));

    ProcessEnvironmentBlock* peb = (ProcessEnvironmentBlock*)m_mainThread->gsBase;
    peb->self = peb;
    peb->executableBaseAddress = 0x80000000;
    peb->sharedDataBase = userSharedDataRegion->Base();
    peb->sharedDataSize = userSharedDataRegion->Size();
    peb->hirakuBase = userSharedDataRegion->Base() + (_hiraku - _user_shared_data);
    peb->hirakuSize = (_hiraku_end - _hiraku);

    if (linkerELFInfo.dynamic) {
        elf64_dynamic_t* dynamic = linkerELFInfo.dynamic;
        elf64_symbol_t* symtab = nullptr;
        elf64_rela_t* plt = nullptr;
        size_t pltSz = 0;
        char* strtab = nullptr;

        while (dynamic->tag != DT_NULL) {
            if (dynamic->tag == DT_PLTRELSZ) {
                pltSz = dynamic->val;
            } else if (dynamic->tag == DT_STRTAB) {
                strtab = (char*)(linkerBaseAddress + dynamic->ptr);
            } else if (dynamic->tag == DT_SYMTAB) {
                symtab = (elf64_symbol_t*)(linkerBaseAddress + dynamic->ptr);
            } else if (dynamic->tag == DT_JMPREL) {
                plt = (elf64_rela_t*)(linkerBaseAddress + dynamic->ptr);
            }

            dynamic++;
        }

        assert(plt && symtab && strtab);

        elf64_rela_t* pltEnd = (elf64_rela_t*)((uintptr_t)plt + pltSz);
        while (plt < pltEnd) {
            if (ELF64_R_TYPE(plt->info) == ELF64_R_X86_64_JUMP_SLOT) {
                long symIndex = ELF64_R_SYM(plt->info);
                elf64_symbol_t sym = symtab[symIndex];

                if (!sym.name) {
                    plt++;
                    continue;
                }

                int binding = ELF64_SYM_BIND(sym.info);
                assert(binding == STB_WEAK);

                char* name = strtab + sym.name;
                if (auto* s = ResolveHirakuSymbol(name)) {
                    Log::Info("%x", peb->hirakuBase + s->address);

                    uintptr_t* p = (uintptr_t*)(linkerBaseAddress + plt->offset);

                    *p = peb->hirakuBase + s->address + plt->addend;
                } else {
                    Log::Error("Failed to resolve program interpreter symbol %s", name);
                }
            }

            plt++;
        }
    }

    char* stackStr = (char*)stack;
    /*for (int i = 0; i < argv.size(); i++) {
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
    }*/

    stackStr -= (uintptr_t)stackStr & 0xf; // align the stack

    stack = (uint64_t*)stackStr;

    // stack -= ((argv.size() + envp.size()) % 2); // If argc + envc is odd then the stack will be misaligned

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

    stack -= sizeof(auxv_t) / sizeof(*stack);
    *((auxv_t*)stack) = {.a_type = AT_SYSINFO_EHDR, .a_val = peb->hirakuBase}; // AT_ENTRY

    /*if (execPath && execPathValue) {
        stack -= sizeof(auxv_t) / sizeof(*stack);
        *((auxv_t*)stack) = {.a_type = AT_EXECPATH, .a_val = (uint64_t)execPathValue}; // AT_EXECPATH
    }*/

    stack--;
    *stack = 0; // null

    /*stack -= envp.size();
    for (int i = 0; i < envp.size(); i++) {
        *(stack + i) = (uint64_t)tempEnvp[i];
    }*/

    stack--;
    *stack = 0; // null

    /*stack -= argv.size();
    for (int i = 0; i < argv.size(); i++) {
        *(stack + i) = (uint64_t)tempArgv[i];
    }*/

    stack--;
    *stack = 0; // argv.size(); // argc

    assert(!((uintptr_t)stack & 0xf));

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
        addressSpace = nullptr;
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
    asm volatile("sti");

    assert(Scheduler::GetCurrentProcess() == this);

    // Check if we are main thread
    Thread* thisThread = Thread::Current();
    if (thisThread != thisThread->parent->GetMainThread().get()) {
        acquireLock(&m_processLock);
        if (m_state != Process_Dying) {
            // Kill the main thread
            thisThread->parent->GetMainThread()->Signal(SIGKILL);
        }

        assert(thisThread->parent == this);

        asm volatile("cli");
        releaseLock(&m_processLock);
        releaseLock(&thisThread->kernelLock);
        asm volatile("sti");
        for (;;)
            Scheduler::Yield();
    }

    assert(m_state == Process_Running);
    m_state = Process_Dying;
    Log::Debug(debugLevelScheduler, DebugLevelNormal, "Killing Process %s (PID %d)", name, m_pid);

    // Ensure the current thread's lock is acquired
    assert(acquireTestLock(&thisThread->kernelLock));

    acquireLock(&m_processLock);
    List<FancyRefPtr<Thread>> runningThreads;

    for (auto& thread : m_threads) {
        if (thread != thisThread && thread) {
            asm("sti");
            acquireLockIntDisable(&thread->stateLock);
            if (thread->state == ThreadStateDying) {
                releaseLock(&thread->stateLock);
                asm("sti");
            }

            if (thread->blocker && thread->state == ThreadStateBlocked) {
                thread->state = ThreadStateZombie;
                releaseLock(&thread->stateLock);
                asm("sti");

                thread->blocker->Interrupt(); // Stop the thread from blocking

                acquireLockIntDisable(&thread->stateLock);
            }

            thread->state = ThreadStateZombie;

            if (!acquireTestLock(&thread->kernelLock)) {
                thread->state =
                    ThreadStateDying; // We have acquired the lock so prevent the thread from getting scheduled
                thread->timeSlice = thread->timeSliceDefault = 0;

                releaseLock(&thread->stateLock);
                asm("sti");
            } else {
                releaseLock(&thread->stateLock);
                asm("sti");

                runningThreads.add_back(thread);
            }
        }
    }

    Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Killing child processes...", m_pid);
    while (m_children.get_length()) {
        // Ensure we release this at least momentarily in case threads are waiting on said lock
        releaseLock(&m_processLock);
        asm("sti");

        FancyRefPtr<Process> child = m_children.get_front();
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "[%d] Killing %d (%s)...", PID(), child->PID(), child->name);
        if (child->State() == Process_Running) {
            child->GetMainThread()->Signal(SIGKILL); // Kill it, burn it with fire
            while (child->State() != Process_Dead)
                Scheduler::Yield(); // Wait for it to die
        } else if (child->State() == Process_Dying) {
            KernelObjectWatcher w;
            child->Watch(w, 0);

            bool wasInterrupted = w.Wait(); // Wait for the process to die
            while (wasInterrupted) {
                wasInterrupted = w.Wait(); // If the parent tried to interrupt us we are dying anyway
            }
        }

        child->m_parent = nullptr;
        acquireLock(&m_processLock);
        m_children.remove(child); // Remove from child list
    }

    // Ensure we release this at least momentarily in case threads are waiting on said lock
    releaseLock(&m_processLock);

    asm("sti");
    Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Killing threads...", m_pid);
    while (runningThreads.get_length()) {
        auto it = runningThreads.begin();
        while (it != runningThreads.end()) {
            FancyRefPtr<Thread> thread = *it;
            if (!acquireTestLock(
                    &thread->kernelLock)) { // Loop through all of the threads so we can acquire their locks
                acquireLockIntDisable(&thread->stateLock);
                thread->state = ThreadStateDying;
                thread->timeSlice = thread->timeSliceDefault = 0;
                releaseLock(&thread->stateLock);
                asm("sti");

                runningThreads.remove(it);

                it = runningThreads.begin();
            } else {
                it++;
            }
        }

        thisThread->Sleep(50000); // Sleep for 50 ms so we do not chew through CPU time
    }

    assert(!runningThreads.get_length());

    // Prevent run queue balancing
    acquireLock(&Scheduler::processesLock);
    acquireLockIntDisable(&m_processLock);

    CPU* cpu = GetCPULocal();
    APIC::Local::SendIPI(cpu->id, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);

    for (auto& t : m_threads) {
        assert(t->parent == this);
        if (t->state != ThreadStateDying && t != thisThread) {
            Log::Error("Thread (%s : %x) TID: %d should be dead, Current Thread (%s : %x) TID: %d", t->parent->name,
                       t.get(), t->tid, thisThread->parent->name, thisThread, thisThread->tid);
            KernelPanic("Thread should be dead");
        }
    }

    acquireLock(&cpu->runQueueLock);

    for (unsigned j = 0; j < cpu->runQueue->get_length(); j++) {
        if (Thread* thread = cpu->runQueue->get_at(j); thread != cpu->currentThread && thread->parent == this) {
            cpu->runQueue->remove_at(j);
            j = 0;
        }
    }

    releaseLock(&cpu->runQueueLock);

    for (unsigned i = 0; i < SMP::processorCount; i++) {
        if (i == cpu->id)
            continue; // Is current processor?

        CPU* other = SMP::cpus[i];
        asm("sti");
        acquireLockIntDisable(&other->runQueueLock);

        if (other->currentThread && other->currentThread->parent == this) {
            assert(other->currentThread->state == ThreadStateDying); // The thread state should be blocked

            other->currentThread = nullptr;
        }

        for (unsigned j = 0; j < other->runQueue->get_length(); j++) {
            Thread* thread = other->runQueue->get_at(j);

            assert(thread);

            if (thread->parent == this) {
                other->runQueue->remove(thread);
                j = 0;
            }
        }

        if (other->currentThread == nullptr) {
            APIC::Local::SendIPI(i, ICR_DSH_SELF, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);
        }

        releaseLock(&other->runQueueLock);
        asm("sti");

        if (other->currentThread == nullptr) {
            APIC::Local::SendIPI(i, ICR_DSH_SELF, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);
        }
    }

    asm("sti");
    releaseLock(&Scheduler::processesLock);

    Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Closing handles...", m_pid);
    m_handles.clear();

    Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Signaling watchers...", m_pid);
    {
        ScopedSpinLock lockWatchers(m_watchingLock);

        // All threads have ceased, set state to dead
        m_state = Process_Dead;

        for (auto* watcher : m_watching) {
            watcher->Signal();
        }
        m_watching.clear();
    }

    if (m_parent && (m_parent->State() == Process_Running)) {
        Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Sending SIGCHILD to %s...", m_pid, m_parent->name);
        m_parent->GetMainThread()->Signal(SIGCHLD);
    }

    // Add to destroyed processes so the reaper thread can safely destroy any last resources
    Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Marking process for destruction...", m_pid);
    Scheduler::MarkProcessForDestruction(this);

    bool isDyingProcess = (thisThread->parent == this);
    if (isDyingProcess) {
        acquireLockIntDisable(&cpu->runQueueLock);
        Log::Debug(debugLevelScheduler, DebugLevelNormal, "[%d] Rescheduling...", m_pid);

        asm volatile("mov %%rax, %%cr3" ::"a"(((uint64_t)Memory::kernelPML4) - KERNEL_VIRTUAL_BASE));

        thisThread->state = ThreadStateDying;
        thisThread->timeSlice = 0;

        releaseLock(&m_processLock);

        // cpu may have changes since we released the processes lock
        // as the run queue
        cpu = GetCPULocal();
        cpu->runQueue->remove(thisThread);
        cpu->currentThread = cpu->idleThread;

        releaseLock(&cpu->runQueueLock);

        Scheduler::DoSwitch(cpu);
        KernelPanic("Dead process attempting to continue execution");
    } else {
        releaseLock(&m_processLock);
    }
}

void Process::Start() {
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

    FancyRefPtr<Process> newProcess = new Process(Scheduler::GetNextPID(), name, workingDirPath, this);
    delete newProcess->addressSpace; // TODO: Do not create address space in first place
    newProcess->addressSpace = addressSpace->Fork();

    newProcess->euid = euid;
    newProcess->uid = uid;
    newProcess->euid = egid;
    newProcess->gid = gid;

    newProcess->m_handles.resize(m_handles.size());
    for (unsigned i = 0; i < m_handles.size(); i++) {
        newProcess->m_handles[i] = m_handles[i];
    }

    m_children.add_back(newProcess);
    Scheduler::RegisterProcess(newProcess);
    return newProcess;
}

pid_t Process::CreateChildThread(uintptr_t entry, uintptr_t stack, uint64_t cs, uint64_t ss) {
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

FancyRefPtr<Thread> Process::GetThreadFromTID_Unlocked(pid_t tid) {
    for (const FancyRefPtr<Thread>& t : m_threads) {
        if (t->tid == tid) {
            return t;
        }
    }

    return nullptr;
}

void Process::MapUserSharedData() {
    FancyRefPtr<UserSharedData> userSharedData;
    {
        ScopedSpinLock<true> lockUSD(userSharedDataLock);
        if (!userSharedDataVMO) {
            userSharedDataVMO = new UserSharedData();
        }

        userSharedData = userSharedDataVMO;
    }

    userSharedDataRegion = addressSpace->MapVMO(static_pointer_cast<VMObject>(userSharedData), 0x7000C00000, true);

    // Allocate space for both a siginfo struct and the signal trampoline
    m_signalTrampoline = addressSpace->AllocateAnonymousVMObject(
        ((signalTrampolineEnd - signalTrampolineStart) + PAGE_SIZE_4K - 1) & ~static_cast<unsigned>(PAGE_SIZE_4K - 1),
        0x7000A00000, false);
    reinterpret_cast<PhysicalVMObject*>(m_signalTrampoline->vmObject.get())
        ->ForceAllocate(); // Forcibly allocate all blocks
    m_signalTrampoline->vmObject->MapAllocatedBlocks(m_signalTrampoline->Base(), GetPageMap());

    // Copy signal trampoline code into process
    asm volatile("cli; mov %%rax, %%cr3" ::"a"(GetPageMap()->pml4Phys));
    memcpy(reinterpret_cast<void*>(m_signalTrampoline->Base()), signalTrampolineStart,
           signalTrampolineEnd - signalTrampolineStart);
    asm volatile("mov %%rax, %%cr3; sti" ::"a"(Scheduler::GetCurrentProcess()->GetPageMap()->pml4Phys));
}