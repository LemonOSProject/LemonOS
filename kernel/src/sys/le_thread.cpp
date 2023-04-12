#include "syscall.h"

#include <lemon/abi/handle.h>
#include <lemon/abi/process.h>

#include <Objects/Process.h>

#include <Scheduler.h>

long le_create_thread(UserPointer<le_handle_t> handle, uint64_t flags, void* entry, void* stack) {
    Process* process = Process::Current();
    
    auto t = process->CreateChildThread(entry, stack);
    
    if(flags & LE_PROCESS_PID) {
        if(handle.StoreValue(t->tid)) {
            return EFAULT;
        }
    } else {
        le_handle_t h = process->AllocateHandle(t, true);
        if(handle.StoreValue(h)) {
            return EFAULT;
        }
    }

    return 0;
}

long le_thread_gettid(le_handle_t handle, UserPointer<pid_t> tid) {
    if(handle == LE_HANDLE_THREAD_SELF) {
        return -Thread::Current()->tid;
    }

    auto thread = KO_GET(Thread, handle);
    return -thread->tid;
}
