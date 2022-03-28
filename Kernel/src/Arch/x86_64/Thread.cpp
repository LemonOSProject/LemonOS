#include <Thread.h>

#include <CPU.h>
#include <Debug.h>
#include <Scheduler.h>
#include <Timer.h>
#include <TimerEvent.h>

#ifdef KERNEL_DEBUG
#include <StackTrace.h>
#endif

#include <bits/posix/posix_signal.h>

void ThreadBlocker::Interrupt() {
    interrupted = true;
    shouldBlock = false;

    acquireLock(&lock);
    if (thread) {
        thread->Unblock();
    }
    releaseLock(&lock);
}

void ThreadBlocker::Unblock() {
    shouldBlock = false;
    removed = true;

    acquireLock(&lock);
    if (thread) {
        thread->Unblock();
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

    ((fx_state_t*)fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
    ((fx_state_t*)fxState)->mxcsrMask = 0xffbf;
    ((fx_state_t*)fxState)->fcw = 0x33f; // Default FPU Control Word State

    kernelStack = Memory::KernelAllocate4KPages(64); // Allocate Memory For Kernel Stack (256KB)
    for (int i = 0; i < 64; i++) {
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),
                                         reinterpret_cast<uintptr_t>(kernelStack) + PAGE_SIZE_4K * i, 1);
    }
    memset(kernelStack, 0, PAGE_SIZE_4K * 64);
    kernelStack = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(kernelStack) + PAGE_SIZE_4K * 64);
}

void Thread::Signal(int signal) {
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
    if (blocker && state == ThreadStateBlocked) {
        blocker->Interrupt(); // Stop the thread from blocking
    }
}

void Thread::HandlePendingSignal(RegisterContext* regs) {
    assert(Scheduler::GetCurrentThread() == this); // Make sure we are this thread

    SignalHandler handler;
    int signal = 0;
    for (int i = 0; i < SIGNAL_MAX; i++) {
        if ((pendingSignals >> i) & 0x1) { // Check if signal bit is set
            signal = i + 1;
            break;
        }
    }

    if (signal == 0) {
        Log::Warning("Thread::HandlePendingSignal: No pending signals!");
        return; // No pending signal
    }

    pendingSignals = pendingSignals & (~(1 << (signal - 1)));

    // These cannot be caught, blocked or ignored
    if (signal == SIGKILL) {
        parent->Die();
        return;
    } else if (signal == SIGSTOP) {
        Log::Error("Thread::HandlePendingSignal: SIGSTOP not handled!");
        return;
    }

    uint64_t oldSignalMask = signalMask;

    handler = parent->signalHandlers[signal - 1];
    if (!(handler.flags & SignalHandler::FlagNoDefer)) { // Do not mask the signal if no defer is set
        signalMask |= 1 << (signal - 1);
    }

    if (handler.action == SignalHandler::HandlerAction::Default) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Thread::HandlePendingSignal: Common action for signal %d",
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
            parent->Die();
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
            Log::Error("Thread::HandlePendingSignal: Unhandled signal %i", signal);
            break;
        }
        return;
    } else if (handler.action == SignalHandler::HandlerAction::Ignore) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Thread::HandlePendingSignal: Ignoring signal %d", signal);
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
    uint64_t* stack = reinterpret_cast<uint64_t*>((regs->rsp & (~0xfULL)) - sizeof(RegisterContext));
    *reinterpret_cast<RegisterContext*>(stack) = *regs;

    *(--stack) = 0; // Pad out the stack
    *(--stack) = oldSignalMask;
    // This could probably be placed in a register but it makes our stack nice and aligned
    *(--stack) = reinterpret_cast<uintptr_t>(handler.userHandler);

    regs->rip = parent->m_signalTrampoline->Base();   // Set instruction pointer to signal trampoline
    regs->rdi = signal;                             // The first argument of the signal handler is the signal number
    if(handler.flags & SignalHandler::FlagSignalInfo){
        /*siginfo_t sigInfo = {
            .si_signo = signal,
            .si_code = 0, // TODO: Signal code?? (for SIGCHLD should be CLD_EXITED or signal that caused it to exit)
            .si_errno = 0,
            .si_pid = 0, // TODO: Sending process PID in here
            .si_uid = 0,
            .si_addr = nullptr, // Fault memory location
            .si_status = 0, // TODO: Exit value or signal
            .si_value = {0}, // TODO: Signal value
        };

        *parent->siginfoPointer = sigInfo;

        regs->rsi = (uintptr_t)parent->siginfoPointer;*/
    }
    regs->rsp = reinterpret_cast<uintptr_t>(stack); // Set rsp to new stack value

    assert(!(regs->rsp & 0xf)); // Ensure that stack is 16-byte aligned
    Log::Debug(debugLevelScheduler, DebugLevelVerbose,
        "Thread::HandlePendingSignal: Executing usermode handler for signal %d", signal);
}

bool Thread::Block(ThreadBlocker* newBlocker) {
    assert(CheckInterrupts());

    acquireLock(&newBlocker->lock);
    acquireLock(&stateLock);

    assert(state != ThreadStateDying);

    asm("cli");
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

    if (pendingSignals & (~EffectiveSignalMask())) {
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

    acquireLock(&stateLock);
    blocker = nullptr;
    releaseLock(&stateLock);

    return newBlocker->WasInterrupted();
}

bool Thread::Block(ThreadBlocker* newBlocker, long& usTimeout) {
    assert(CheckInterrupts());

    Timer::TimerCallback timerCallback = [](void* t) -> void {
        reinterpret_cast<Thread*>(t)->blockTimedOut = true;
        reinterpret_cast<Thread*>(t)->Unblock();
    };

    acquireLock(&newBlocker->lock);
    acquireLock(&stateLock);

    asm("cli");
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

    if (pendingSignals & (~EffectiveSignalMask())) {
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
            Unblock();

            asm("sti");

            blocker->Interrupt(); // If the blocker re-calls Thread::Unblock that's ok
        }
    }

    if (blockTimedOut) {
        blocker->Interrupt();
        usTimeout = 0;
    }

    return (!blockTimedOut) && newBlocker->WasInterrupted();
}

void Thread::Sleep(long us) {
    assert(CheckInterrupts());

    blockTimedOut = false;

    Timer::TimerCallback timerCallback = [](void* t) -> void {
        reinterpret_cast<Thread*>(t)->blockTimedOut = true;
        reinterpret_cast<Thread*>(t)->Unblock();
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
            Unblock();

            asm("sti");
        }
    }
}

void Thread::Unblock() {
    assert(state != ThreadStateDying);
    timeSlice = timeSliceDefault;

    if (state != ThreadStateZombie)
        state = ThreadStateRunning;
}