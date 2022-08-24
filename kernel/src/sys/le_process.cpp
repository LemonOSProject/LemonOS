#include "syscall.h"

#include <abi/process.h>

#include <Objects/Process.h>

long le_create_process(UserPointer<le_handle_t> handle, uint64_t flags, le_str_t name) {
    Process* process = Process::Current();
    FancyRefPtr<Process> newProcess;
    
    if(flags & LE_PROCESS_FORK) {
        if(!(flags & LE_PROCESS_DAEMON)) {
            // For now LE_PROCESS_DAEMON must be set meaning that
            // the process is not killed when the handle is closed
            Log::Error("le_create_process: LE_PROCESS_DAEMON must be set.");
            return EINVAL;
        }

        newProcess = process->Fork();

        Thread* t = Thread::Current();
        Thread* nt = newProcess->GetMainThread();

        void* kStack = nt->kernelStack;
        void* kStackBase = nt->kernelStackBase;
        void* fxState = nt->fxState;

        *nt = *t;

        nt->tid = 1;
        nt->kernelStack = kStack;
        nt->kernelStackBase = kStackBase;
        nt->fxState = fxState;

        nt->registers.rax = ECHILD;

        le_handle_t pHandle = process->AllocateHandle(newProcess, flags & LE_PROCESS_CLOEXEC);

        if(handle.StoreValue(pHandle)) {
            Log::Warning("le_create_process: Handle gets leaked on EFAULT");
            return EFAULT;
        }

        return 0;
    } else {
        // For now all process creation must be a fork
        Log::Error("le_create_process: LE_PROCESS_FORK must be set.");
        return EINVAL;
    }
}

long le_process_getpid(le_handle_t process) {

}

long le_process_mem_poke(le_handle_t process) {

}

long le_process_mem_write(le_handle_t process) {

}
