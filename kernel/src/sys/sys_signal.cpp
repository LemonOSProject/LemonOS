#include "syscall.h"

#include <Objects/Process.h>
#include <Thread.h>

#include <Logging.h>
#include <Scheduler.h>
#include <Thread.h>

#include <lemon/abi/signal.h>

SYSCALL long sys_sigprocmask(int how, UserPointer<sigset_t> _set, UserPointer<sigset_t> _oldset) {
    Thread* t = Thread::current();

    sigset_t oldset;

    sigset_t set;
    if (_set.get(set))
        return EFAULT;

    if (how == SIG_BLOCK) {
        oldset = t->signalMask;
        t->signalMask |= set;
    } else if (how == SIG_UNBLOCK) {
        oldset = t->signalMask;
        t->signalMask &= ~(set);
    } else if (how == SIG_SETMASK) {
        oldset = t->signalMask;
        t->signalMask = set;
    } else {
        return EINVAL;
    }

    if (_oldset.ptr() && _oldset.store(oldset)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_sigaction(int sig, UserPointer<struct sigaction> _sigaction,
                           UserPointer<struct sigaction> _oldaction) {
    // Behaviour of some signals such as SIGKILL can't be changed
    if ((1UL << (sig - 1)) & UNMASKABLE_SIGNALS) {
        Log::Warning("sys_sigaction: signal %d is unmaskable!", sig);
        return EINVAL;
    }

    Thread* t = Thread::current();
    Process* proc = Process::current();

    // TODO: signal handlers aren't protected by locks!!!!
    SignalHandler& handler = proc->signalHandlers[sig - 1];

    struct sigaction oldaction;
    oldaction.sa_handler = (void (*)(int))handler.userHandler;
    oldaction.sa_mask = handler.mask;
    oldaction.sa_flags = handler.flags;

    struct sigaction action;
    if (_sigaction.get(action))
        return EFAULT;

    handler.userHandler = (void*)action.sa_handler;
    handler.mask = action.sa_mask;
    handler.flags = action.sa_flags;

    if(action.sa_handler == SIG_DFL) {
        handler.action = SignalHandler::HandlerAction::Default;
    } else if(action.sa_handler == SIG_IGN) {
        handler.action = SignalHandler::HandlerAction::Ignore;
    } else {
        handler.action = SignalHandler::HandlerAction::UsermodeHandler;
    }

    if (action.sa_flags & ~(SignalHandler::supportedFlags)) {
        Log::Warning("sys_sigaction: Unsupported handler flags %x", action.sa_flags & ~(SignalHandler::supportedFlags));
        return EINVAL;
    }

    if(_oldaction.ptr() && _oldaction.store(oldaction)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_kill(pid_t pid, pid_t tid, int signal) {
    if (pid == 0) {
        if (tid == -1) {
            Thread::current()->signal(signal);
            return 0;
        }

        Process* proc = Process::current();

        auto t = proc->get_thread_from_tid(tid);
        if (!t) {
            return ESRCH;
        }
    } else if (pid == -1) {
        Process* cproc = Process::current();
    }

    Process* cproc = Process::current();
    auto proc = Scheduler::FindProcessByPID(pid);
    if (!proc) {
        return ESRCH;
    }

    Log::Warning("sending signals to other processes is unimplemented!");

    return ENOSYS;
}

SYSCALL long sys_sigaltstack(UserPointer<const stack_t> stack, UserPointer<stack_t> oldstack) {
    Log::Warning("sys_sigaltstack is a stub!");
    return ENOSYS;
}

SYSCALL long sys_sigsuspend(UserPointer<const sigset_t> sigmask) {
    Log::Warning("sys_sigsuspend is a stub!");
    return ENOSYS;
}

SYSCALL long sys_sigreturn() {
    Thread* th = Thread::current();
    uint64_t* threadStack = reinterpret_cast<uint64_t*>(th->scRegisters->rsp);

    return ENOSYS;
}
