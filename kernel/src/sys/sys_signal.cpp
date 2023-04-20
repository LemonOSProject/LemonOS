#include "syscall.h"

#include <Objects/Process.h>

#include <Scheduler.h>
#include <Thread.h>
#include <Logging.h>

SYSCALL long sys_sigprocmask(int how, UserPointer<const sigset_t> set, UserPointer<sigset_t> oldset) {

}

SYSCALL long sys_sigaction(int sig, UserPointer<const struct sigaction> sigaction,
                           UserPointer<struct sigaction> oldaction) {
    
}

SYSCALL long sys_kill(pid_t pid, pid_t tid, int signal) {
    if(pid == 0) {
        if(tid == -1) {
            Thread::current()->signal(signal);
            return 0;
        }

        Process* proc = Process::current();
        
        auto t = proc->get_thread_from_tid(tid));
        if(!t) {
            return ESRCH;
        }
    } else if(pid == -1) {
        Process* cproc = Process::current();
        
    }

    Process* cproc = Process::current();
    auto proc = Scheduler::FindProcessByPID(pid);
    if(!proc) {
        return ESRCH;
    }

    return 0;
}

SYSCALL long sys_sigaltstack(UserPointer<const stack_t> stack, UserPointer<stack_t> oldstack) {
    Log::Warning("sys_sigaltstack is a stub!");
    return ENOSYS;
}

SYSCALL long sys_sigsuspend(UserPointer<const sigset_t> sigmask) {
    Log::Warning("sys_sigsuspend is a stub!");
    return ENOSYS;
}
