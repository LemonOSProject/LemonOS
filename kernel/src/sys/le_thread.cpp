#include "syscall.h"

#include <lemon/abi/handle.h>
#include <lemon/abi/process.h>

#include <Objects/Process.h>

#include <Scheduler.h>

SYSCALL long le_create_thread(UserPointer<le_handle_t> handle, uint64_t flags, void* entry, void* stack) {
    Process* process = Process::current();
    
    auto t = process->create_child_thread(entry, stack);
    
    if(flags & LE_PROCESS_PID) {
        if(handle.StoreValue(t->tid)) {
            return EFAULT;
        }
    } else {
        le_handle_t h = process->allocate_handle(t, true);
        if(handle.StoreValue(h)) {
            return EFAULT;
        }
    }

    return 0;
}

SYSCALL long le_thread_gettid(le_handle_t handle, UserPointer<pid_t> tid) {
    if(handle == LE_HANDLE_THREAD_SELF) {
        return -Thread::current()->tid;
    }

    auto thread = KO_GET(Thread, handle);
    return -thread->tid;
}
