#include <Thread.h>

#include <CPU.h>
#include <Debug.h>
#include <Scheduler.h>
#include <Timer.h>
#include <TimerEvent.h>

#ifdef KERNEL_DEBUG
#include <StackTrace.h>
#endif

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

void Thread::Signal(int signal) {
    SignalHandler& sigHandler = parent->signalHandlers[signal - 1];
    if (sigHandler.action == SignalHandler::ActionIgnore) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Ignoring signal %d!", signal);
        return;
    }

    pendingSignals |= 1 << (signal - 1); // Set corresponding bit for signal

    // TODO: Race condition?
    if (blocker && state == ThreadStateBlocked) {
        blocker->Interrupt(); // Stop the thread from blocking
    }
}

void Thread::HandlePendingSignal(RegisterContext* regs) {
    assert(Scheduler::GetCurrentThread() == this); // Make sure we are this thread
    assert(!CheckInterrupts());                    // Assume interrupts are disabled

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
        Scheduler::EndProcess(parent);
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

    if (handler.action == SignalHandler::ActionDefault) {
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
            Scheduler::EndProcess(parent);
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
    } else if (handler.action == SignalHandler::ActionIgnore) {
        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Thread::HandlePendingSignal: Ignoring signal %d", signal);
        return;
    }

    // User handler
    assert(handler.action == SignalHandler::ActionUsermodeHandler);

    // On the usermode stack:
    // Save the register state
    // Save the signal mask
    uint64_t* stack = reinterpret_cast<uint64_t*>(regs->rsp - sizeof(RegisterContext));
    *reinterpret_cast<RegisterContext*>(stack) = *regs;

    *(--stack) = oldSignalMask;
    // This could probably be placed in a register but it makes our stack nice and aligned
    *(--stack) = reinterpret_cast<uintptr_t>(handler.userHandler);

    regs->rip = parent->signalTrampoline->Base();   // Set instruction pointer to signal trampoline
    regs->rdi = signal;                             // The first argument of the signal handler is the signal number
    regs->rsp = reinterpret_cast<uintptr_t>(stack); // Set rsp to new stack value

    assert(!(regs->rsp & 0xf)); // Ensure that stack is 16-byte aligned
    Log::Debug(debugLevelScheduler, DebugLevelVerbose,
               "Thread::HandlePendingSignal: Executing usermode handler for signal %d", signal);
}

bool Thread::Block(ThreadBlocker* newBlocker) {
    assert(CheckInterrupts());

    acquireLock(&newBlocker->lock);

    asm("cli");
    newBlocker->thread = this;
    if (!newBlocker->ShouldBlock()) {
        releaseLock(&newBlocker->lock); // We were unblocked before the thread was actually blocked
        asm("sti");

        return false;
    }

    if (pendingSignals & (~signalMask)) {
        releaseLock(&newBlocker->lock); // Pending signals, don't block
        asm("sti");

        return true; // We were interrupted by a signal
    }

    blocker = newBlocker;

    releaseLock(&newBlocker->lock);
    state = ThreadStateBlocked;
    asm("sti");

    Scheduler::Yield();

    blocker = nullptr;

    return newBlocker->WasInterrupted();
}

bool Thread::Block(ThreadBlocker* newBlocker, long& usTimeout) {
    assert(CheckInterrupts());

    Timer::TimerCallback timerCallback = [](void* t) -> void {
        reinterpret_cast<Thread*>(t)->blockTimedOut = true;
        reinterpret_cast<Thread*>(t)->Unblock();
    };

    acquireLock(&newBlocker->lock);
    if (!newBlocker->ShouldBlock()) {
        releaseLock(&newBlocker->lock); // We were unblocked before the thread was actually blocked

        return false;
    }

    blockTimedOut = false;
    blocker = newBlocker;

    blocker->thread = this;

    {
        Timer::TimerEvent ev(usTimeout, timerCallback, this);

        asm("cli");
        releaseLock(&newBlocker->lock);
        state = ThreadStateBlocked;

        // Now that the thread state has been set blocked, check if we timed out before setting to blocked
        if (!blockTimedOut) {
            asm("sti");

            Scheduler::Yield();
        } else {
            state = ThreadStateRunning;

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

        asm("cli");
        state = ThreadStateBlocked;

        // Now that the thread state has been set blocked, check if we timed out before setting to blocked
        if (!blockTimedOut) {
            asm("sti");

            Scheduler::Yield();
        } else {
            state = ThreadStateRunning;

            asm("sti");
        }
    }
}

void Thread::Unblock() {
    timeSlice = timeSliceDefault;

    if (state != ThreadStateZombie)
        state = ThreadStateRunning;
}