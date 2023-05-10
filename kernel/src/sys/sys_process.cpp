#include "syscall.h"

#include <abi/process.h>
#include <abi/wait.h>

#include <Objects/Process.h>

SYSCALL long sys_waitpid(UserPointer<pid_t> _pid, UserPointer<int> wstatus, int options) {
    Process* process = Process::current();

    pid_t pid;
    if(_pid.get(pid))
        return EFAULT;

    if(pid == -1) {
        // Wait for any child process
        FancyRefPtr<Process> child = process->remove_dead_child();
        if(child) {
            if(wstatus) {
                SC_USER_STORE(wstatus, child->exit_status());
            }
            SC_USER_STORE(_pid, child->pid());
            return 0;
        }

        if(options & WNOHANG) {
            return 0;
        }

        if(Error e = process->wait_for_child_to_die(child); e) {
            // May return ECHILD or EINTR
            return e;
        }

        if(wstatus) {
            SC_USER_STORE(wstatus, child->exit_status());
        }
        SC_USER_STORE(_pid, child->pid());
        return 0;
    } else if(pid == 0) {
        // Wait for any child process whose pgid is equal to this process' pgid
        Log::Warning("sys_waitpid: process groups are unimplemented!");
        return ECHILD;
    } else if(pid < -1) {
        // Wait for any child process whose pgid is equal to abs(pid)
        Log::Warning("sys_waitpid: process groups are unimplemented!");
        return ECHILD;
    }
    
    FancyRefPtr<Process> child = process->find_child_by_pid(pid);
    if(!child.get()) {
        return ECHILD;
    }

    // TODO: state race condition
    if(child->is_dead()) {
        process->remove_dead_child(child->pid());

        if(wstatus) {
            SC_USER_STORE(wstatus, child->exit_status());
        }
        SC_USER_STORE(_pid, child->pid());
        return 0;
    }

    if(options & WNOHANG) {
        return 0;
    }

    KernelObjectWatcher w;
    w.Watch(child, KOEvent::ProcessTerminated);

    if(w.wait()) {
        return EINTR;
    }

    assert(child->is_dead());
    process->remove_dead_child(child->pid());

    if(wstatus) {
        SC_USER_STORE(wstatus, child->exit_status());
    }
    SC_USER_STORE(_pid, child->pid());

    return 0;
}

SYSCALL long sys_exit(int status) {
    Process* process = Process::current();
    process->exitCode = status;

    process->die();

    assert(0);
}
