#include <Thread.h>

#include <MM/UserAccess.h>

#include <CPU.h>
#include <Debug.h>
#include <Scheduler.h>
#include <Timer.h>
#include <TimerEvent.h>

#ifdef KERNEL_DEBUG
#include <StackTrace.h>
#endif

#include <abi/signal.h>

void ThreadBlocker::interrupt() {
    interrupted = true;
    shouldBlock = false;

    acquireLock(&lock);
    if (thread) {
        thread->unblock();
    }
    releaseLock(&lock);
}

void ThreadBlocker::unblock() {
    shouldBlock = false;
    removed = true;

    acquireLock(&lock);
    if (thread) {
        thread->unblock();
    }
    releaseLock(&lock);
}

Thread::Thread(Process* _parent, pid_t _tid)
    : parent(_parent), tid(_tid), state(ThreadStateRunning) {
    memset(&registers, 0, sizeof(RegisterContext));
    registers.rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
    registers.cs = KERNEL_CS; // Kernel CS
    registers.ss = KERNEL_SS; // Kernel SS

    fxState = Memory::KernelAllocate4KPages(1); // Allocate Memory for the FPU/Extended Register State
    Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)fxState, 1);

    memset(fxState, 0, 4096);
    ((fx_state_t*)fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
    ((fx_state_t*)fxState)->mxcsrMask = 0xffbf;
    ((fx_state_t*)fxState)->fcw = 0x33f; // Default FPU Control Word State

    kernelStackBase = kmalloc(524288);
    kernelStack = (uint8_t*)kernelStackBase + 524488;
}

Thread::~Thread() {
    
}

void Thread::signal(int signal) {
    SignalHandler& sigHandler = parent->signalHandlers[signal - 1];
    if (sigHandler.action == SignalHandler::HandlerAction::Ignore) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Ignoring signal %d!", signal);
        return;
    } else if(sigHandler.action == SignalHandler::HandlerAction::Default){
        if(DefaultActionForSignal(signal) == SignalAction::Ignore){
            return;
        }
    }

    pendingSignals |= 1 << (signal - 1); // Set corresponding bit for signal

    // TODO: Race condition?
    acquireLock(&stateLock);
    if (blocker && state == ThreadStateBlocked) {
        releaseLock(&stateLock);

        blocker->interrupt(); // Stop the thread from blocking
    } else {
        releaseLock(&stateLock);
    }
}

void Thread::handle_pending_signal(RegisterContext* regs) {
    assert(Thread::current() == this); // Make sure we are this thread

    SignalHandler handler;
    int signal = 0;
    for (int i = 0; i < SIGNAL_MAX; i++) {
        if ((pendingSignals >> i) & 0x1) { // Check if signal bit is set
            signal = i + 1;
            break;
        }
    }

    if (signal == 0) {
        Log::Warning("Thread::handle_pending_signal: No pending signals!");
        return; // No pending signal
    }

    pendingSignals = pendingSignals & (~(1 << (signal - 1)));

    // These cannot be caught, blocked or ignored
    if (signal == SIGKILL) {
        parent->die();
        return;
    } else if (signal == SIGSTOP) {
        Log::Error("Thread::handle_pending_signal: SIGSTOP not handled!");
        return;
    }

    uint64_t oldSignalMask = signalMask;

    handler = parent->signalHandlers[signal - 1];
    if (!(handler.flags & SignalHandler::FlagNoDefer)) { // Do not mask the signal if no defer is set
        signalMask |= 1 << (signal - 1);
    }

    if (handler.action == SignalHandler::HandlerAction::Default) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Thread::handle_pending_signal: Common action for signal %d",
                   signal);
        // Default action
        switch (signal) {
            // Terminate
        case SIGABRT:
        case SIGALRM:
        case SIGBUS:
        case SIGFPE:
        case SIGHUP:
        case SIGILL:
        case SIGINT: // Keyboard interrupt
        case SIGIO:
        case SIGKILL:
        case SIGPIPE:
        case SIGPWR:
        case SIGQUIT:
        case SIGSEGV:
        case SIGSTKFLT:
            parent->die();
            return;
            // Ignore
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
        case SIGUSR1:
        case SIGUSR2:
            break;
            // Unhandled
        case SIGCONT:
        case SIGSTOP:
        default:
            Log::Error("Thread::handle_pending_signal: Unhandled signal %i", signal);
            break;
        }
        return;
    } else if (handler.action == SignalHandler::HandlerAction::Ignore) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Thread::handle_pending_signal: Ignoring signal %d", signal);
        return;
    }

    // User handler
    assert(handler.action == SignalHandler::HandlerAction::UsermodeHandler);

    // On the usermode stack:
    // Save the register state
    // Save the signal mask
    //uint64_t* stack = reinterpret_cast<uint64_t*>(regs->rsp - sizeof(ucontext_t));
    //ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(stack);

    // Ensure stack alignment
    // Make sure to subtract the 128-byte redzone
    uint64_t* stack = reinterpret_cast<uint64_t*>((regs->rsp & (~0xfULL)) - 128 - sizeof(RegisterContext));
    *reinterpret_cast<RegisterContext*>(stack) = *regs;
    

    *(--stack) = 0; // Pad out the stack
    
    // Save FP regs
    stack -= 512 / sizeof(uint64_t);

    // fxsave64 needs this to be 16-byte aligned
    char fpState[4096 + 15];
    char* fpStateAligned = (char*)(((uintptr_t)fpState + 15) & (~0xfULL));

    asm volatile("fxsave64 (%0)" ::"r"(fpStateAligned) : "memory");

    // TODO: Some way to register a fault handler and access user memory directly in this funciton would be real nice
    if(user_memcpy(stack, fpStateAligned, 4096)) {
        parent->die();
        return;
    }

    uint64_t sigstack[] = {
        oldSignalMask,
        // This could probably be placed in a register but it makes our stack nice and aligned
        (uint64_t)handler.userHandler,
    };

    if(user_memcpy(stack, sigstack, sizeof(sigstack))) {
        parent->die();
        return;
    }

    regs->rsp = reinterpret_cast<uintptr_t>(stack); // Set rsp to new stack value
    if(handler.flags & SignalHandler::FlagSignalInfo){
        siginfo_t sigInfo = {};

        sigInfo.si_signo = signal;

        // Keep stack alignment
        regs->rsp -= (sizeof(siginfo_t) + 0xf) & (~0xfULL);
        *(siginfo_t*)regs->rsp = sigInfo;

        regs->rsi = regs->rsp;
    }

    regs->rip = parent->m_signalTrampoline->Base();   // Set instruction pointer to signal trampoline
    regs->rdi = signal;                             // The first argument of the signal handler is the signal number

    assert(!(regs->rsp & 0xf)); // Ensure that stack is 16-byte aligned
    Log::Debug(debugLevelScheduler, DebugLevelNormal,
        "Thread::handle_pending_signal: Executing usermode handler for signal %d", signal);

    return;
}

bool Thread::block(ThreadBlocker* newBlocker) {
    assert(CheckInterrupts());

    acquireLock(&newBlocker->lock);
    acquireLockIntDisable(&stateLock);

    assert(state != ThreadStateDying);

    newBlocker->thread = this;
    if (!newBlocker->ShouldBlock()) {
        releaseLock(&newBlocker->lock); // We were unblocked before the thread was actually blocked
        releaseLock(&stateLock);
        asm("sti");

        return false;
    }

    if(state == ThreadStateZombie) {
        releaseLock(&newBlocker->lock); // Pending thread destruction, don't block
        releaseLock(&stateLock);
        asm("sti");
        return true;
    }

    if (pendingSignals & (~effective_signal_mask())) {
        releaseLock(&newBlocker->lock); // Pending signals, don't block
        releaseLock(&stateLock);
        asm("sti");

        return true; // We were interrupted by a signal
    }

    blocker = newBlocker;

    releaseLock(&newBlocker->lock);
    
    state = ThreadStateBlocked;
    releaseLock(&stateLock);
    asm("sti");

    Scheduler::Yield();

    acquireLockIntDisable(&stateLock);
    blocker = nullptr;
    releaseLock(&stateLock);
    asm("sti");

    return newBlocker->WasInterrupted();
}

bool Thread::block(ThreadBlocker* newBlocker, long& usTimeout) {
    assert(CheckInterrupts());

    Timer::TimerCallback timerCallback = [](void* t) -> void {
        reinterpret_cast<Thread*>(t)->blockTimedOut = true;
        reinterpret_cast<Thread*>(t)->unblock();
    };

    acquireLock(&newBlocker->lock);
    acquireLockIntDisable(&stateLock);

    newBlocker->thread = this;
    if (!newBlocker->ShouldBlock()) {
        releaseLock(&newBlocker->lock); // We were unblocked before the thread was actually blocked
        releaseLock(&stateLock);
        asm("sti");

        return false;
    }

    if(state == ThreadStateZombie) {
        releaseLock(&newBlocker->lock); // Pending thread destruction, don't block
        releaseLock(&stateLock);
        asm("sti");
        return true;
    }

    if (pendingSignals & (~effective_signal_mask())) {
        releaseLock(&newBlocker->lock); // Pending signals, don't block
        releaseLock(&stateLock);
        asm("sti");

        return true; // We were interrupted by a signal
    }

    blockTimedOut = false;
    blocker = newBlocker;

    {
        asm("sti");
        Timer::TimerEvent ev(usTimeout, timerCallback, this);
        asm("cli");

        releaseLock(&newBlocker->lock);
        state = ThreadStateBlocked;
        releaseLock(&stateLock);

        // Now that the thread state has been set blocked, check if we timed out before setting to blocked
        if (!blockTimedOut) {
            asm("sti");

            Scheduler::Yield();
        } else {
            unblock();

            asm("sti");

            blocker->interrupt(); // If the blocker re-calls Thread::Unblock that's ok
        }
    }

    if (blockTimedOut) {
        blocker->interrupt();
        usTimeout = 0;
    }

    return (!blockTimedOut) && newBlocker->WasInterrupted();
}

void Thread::sleep(long us) {
    assert(CheckInterrupts());

    blockTimedOut = false;

    Timer::TimerCallback timerCallback = [](void* t) -> void {
        reinterpret_cast<Thread*>(t)->blockTimedOut = true;
        reinterpret_cast<Thread*>(t)->unblock();
    };

    {
        Timer::TimerEvent ev(us, timerCallback, this);

        acquireLockIntDisable(&stateLock);
        assert(state != ThreadStateDying);
        if(state == ThreadStateZombie) {
            blockTimedOut = true;
        } else {
            state = ThreadStateBlocked;
        }
        releaseLock(&stateLock);

        // Now that the thread state has been set blocked, check if we timed out before setting to blocked
        if (!blockTimedOut) {
            asm("sti");

            Scheduler::Yield();
        } else {
            unblock();

            asm("sti");
        }
    }
}

void Thread::unblock() {
    assert(state != ThreadStateDying);
    bool intsWereEnabled = CheckInterrupts();
    if(intsWereEnabled)
        acquireLockIntDisable(&stateLock);
    else
        acquireLock(&stateLock);
    timeSlice = timeSliceDefault;

    if (state != ThreadStateZombie)
        state = ThreadStateRunning;

    releaseLock(&stateLock);
    if(intsWereEnabled)
        asm volatile("sti");
}
