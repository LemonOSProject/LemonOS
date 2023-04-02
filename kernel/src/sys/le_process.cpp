#include "syscall.h"

#include <abi/process.h>

#include <Objects/Process.h>

long le_create_process(UserPointer<le_handle_t> handle, uint64_t flags, le_str_t name) {
    Process* process = Process::Current();
    FancyRefPtr<Process> newProcess;

    if (flags & LE_PROCESS_FORK) {
        if (flags & LE_PROCESS_PID) {
            if (!(flags & LE_PROCESS_DETACH)) {
                Log::Error("le_create_process: LE_PROCESS_DETACH must be set with LE_PROCESS_PID.");
                return EINVAL;
            }

            // Store zero before the address space is forked,
            // the child will know its the child when the returned value is 0
            if (handle.StoreValue(0)) {
                return EFAULT;
            }
        } else  {
            if (!(flags & LE_PROCESS_DETACH)) {
                // For now LE_PROCESS_DETACH must be set meaning that
                // the process is not killed when the handle is closed
                Log::Error("le_create_process: LE_PROCESS_DETACH must be set.");
                return EINVAL;
            }

            // Store zero before the address space is forked,
            // the child will know its the child when the returned value is LE_PROCESS_IS_CHILD
            if (handle.StoreValue(LE_PROCESS_IS_CHILD)) {
                return EFAULT;
            }
        }

        String newName = process->name;
        if (name) {
            newName = get_user_string_or_fault(name, NAME_MAX+1);
            if (newName.Length() > NAME_MAX) {
                return ENAMETOOLONG;
            }
        }

        newProcess = process->Fork();
        strcpy(newProcess->name, newName.c_str());

        Thread* t = Thread::Current();
        auto nt = newProcess->GetMainThread();

        nt->gsBase = t->gsBase;
        nt->fsBase = t->fsBase;

        nt->pendingSignals = t->pendingSignals;
        nt->signalMask = t->signalMask;

        nt->tid = 1;

        nt->registers = *t->scRegisters;
        memcpy(nt->fxState, t->fxState, PAGE_SIZE_4K);

        // Returns ECHILD from the syscall
        nt->registers.rax = ECHILD;

        nt->kernelLock = 0;

        if (flags & LE_PROCESS_PID) {
            if(handle.StoreValue(process->PID())) {
                Log::Warning("le_create_process: PID gets leaked on EFAULT");
                return EFAULT;
            }
        } else {
            le_handle_t pHandle = process->AllocateHandle(newProcess, flags & LE_PROCESS_CLOEXEC);
            if (handle.StoreValue(pHandle)) {
                Log::Warning("le_create_process: Handle gets leaked on EFAULT");
                return EFAULT;
            }
        }

        newProcess->Start();
        return 0;
    } else {
        // For now all process creation must be a fork
        Log::Error("le_create_process: LE_PROCESS_FORK must be set.");
        return EINVAL;
    }
}

long le_start_process(le_handle_t handle) {
    Process* self = Process::Current();
    auto proc = SC_TRY_OR_ERROR(self->GetHandleAs<Process>(handle));

    proc->Start();

    return 0;
}

long le_process_getpid(le_handle_t handle, UserPointer<pid_t> pid) {
    Process* self = Process::Current();
    auto proc = SC_TRY_OR_ERROR(self->GetHandleAs<Process>(handle));

    SC_USER_STORE(pid, proc->PID());

    return 0;
}

long le_process_mem_poke(le_handle_t process, void* destination, void* virtualAddress, uintptr_t size) { return ENOSYS; }

long le_process_mem_write(le_handle_t process, void* virtualAddress, void* source, uintptr_t size) { return ENOSYS; }
