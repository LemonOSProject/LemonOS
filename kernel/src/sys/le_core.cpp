#include "syscall.h"

#include <Timer.h>
#include <Objects/Process.h>

#include <Logging.h>

struct ModuleQuery {
    char name[256];
};

struct DeviceQuery {
    char name[256];
};

SYSCALL long le_log(le_str_t umsg) {
    String msg = get_user_string_or_fault(umsg, 0xffffffff);
    Process* process = Process::Current(); 

    Log::Info("[%s] %s", process->name, msg.c_str());
    return 0;
}

SYSCALL long le_boot_timer() {
    return Timer::UsecondsSinceBoot();
}

SYSCALL long le_handle_close(le_handle_t handle) {
    Process* process = Process::Current();

    return process->DestroyHandle(handle);
}

SYSCALL long le_handle_dup(le_handle_t handle, int flags) {
    return ENOSYS;
}

SYSCALL long le_futex_wait(UserPointer<int> futex, int expected, const struct timespec* time) {
    return 0; //ENOSYS;
}

SYSCALL long le_futex_wake(UserPointer<int> futex) {
    return 0; //ENOSYS;
}

SYSCALL long le_set_user_tcb(uintptr_t value) {
    Thread* thread = Thread::Current();

    asm("cli");
    thread->fsBase = value;

    UpdateUserTCB(thread->fsBase);
    asm("sti");

    return 0;
}

long le_load_module(le_str_t path) {
    return ENOSYS;
}

long le_unload_module(le_str_t name) {
    return ENOSYS;
}

long le_query_modules() {
    return ENOSYS;
}

long le_query_devices() {
    return ENOSYS;
}
