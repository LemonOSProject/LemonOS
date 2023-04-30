#include "syscall.h"

#include <CPU.h>
#include <arch_syscall.h>

#include <Logging.h>
#include <Scheduler.h>

#define MSR_STAR 0xC000'0081
#define MSR_LSTAR 0xC000'0082
#define MSR_CSTAR 0xC000'0083
#define MSR_SFMASK 0xC000'0084

extern "C" void syscall_entry();

void syscall_init() {
    uint64_t low = ((uint64_t)syscall_entry) & 0xFFFFFFFF;
    uint64_t high = ((uint64_t)syscall_entry) >> 32;
    asm volatile("wrmsr" ::"a"(low), "d"(high), "c"(MSR_LSTAR));

    // User CS selector set to this field + 16, SS this field + 8
    uint32_t sysretSelectors = USER_SS - 8;
    // When syscall is called, CS set to field, SS this field + 8
    uint32_t syscallSelectors = KERNEL_CS;
    asm volatile("wrmsr" ::"a"(0), "d"((sysretSelectors << 16) | syscallSelectors), "c"(MSR_STAR));
    // SFMASK masks flag values
    // mask everything except reserved flags to disable IRQs when syscall is called
    asm volatile("wrmsr" ::"a"(0x3F7FD5U), "d"(0), "c"(MSR_SFMASK));

    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000080));
    low |= 1; // SCE (syscall enable)
    asm volatile("wrmsr" ::"a"(low), "d"(high), "c"(0xC0000080));
}

void DumpLastSyscall(Thread*) {
    /*RegisterContext& lastSyscall = t->lastSyscall.regs;
    Log::Info("Last syscall:\nCall: %d, arg0: %i (%x), arg1: %i (%x), arg2: %i (%x), arg3: %i (%x), arg4: %i (%x), "
              "arg5: %i (%x) result: %i (%x)",
              lastSyscall.rax, SC_ARG0(&lastSyscall), SC_ARG0(&lastSyscall), SC_ARG1(&lastSyscall),
              SC_ARG1(&lastSyscall), SC_ARG2(&lastSyscall), SC_ARG2(&lastSyscall), SC_ARG3(&lastSyscall),
              SC_ARG3(&lastSyscall), SC_ARG4(&lastSyscall), SC_ARG4(&lastSyscall), SC_ARG5(&lastSyscall),
              SC_ARG5(&lastSyscall), t->lastSyscall.result, t->lastSyscall.result);*/
}

#define SYS_SYSCALLS 19
#define LE_SYSCALLS 10

DFN_SYSCALL(le_log);
DFN_SYSCALL(le_boot_timer);
DFN_SYSCALL(le_handle_close);
DFN_SYSCALL(le_handle_dup);
DFN_SYSCALL(le_futex_wait);
DFN_SYSCALL(le_futex_wake);
DFN_SYSCALL(le_set_user_tcb);
DFN_SYSCALL(le_create_process);
DFN_SYSCALL(le_start_process);
DFN_SYSCALL(le_create_thread);
DFN_SYSCALL(le_nanosleep);

DFN_SYSCALL(sys_read);
DFN_SYSCALL(sys_write);
DFN_SYSCALL(sys_openat);
DFN_SYSCALL(sys_fstatat);
DFN_SYSCALL(sys_lseek);
DFN_SYSCALL(sys_mmap);
DFN_SYSCALL(sys_mprotect);
DFN_SYSCALL(sys_munmap);
DFN_SYSCALL(sys_ioctl);
DFN_SYSCALL(sys_pread);
DFN_SYSCALL(sys_pwrite);
DFN_SYSCALL(sys_execve);
DFN_SYSCALL(sys_sigprocmask);
DFN_SYSCALL(sys_sigaction);
DFN_SYSCALL(sys_kill);
DFN_SYSCALL(sys_sigreturn);
DFN_SYSCALL(sys_poll);
DFN_SYSCALL(sys_chdir);
DFN_SYSCALL(sys_getcwd);

#define SC(x) ((void*)&x)

void* sysTable[] = {
    SC(sys_read),      SC(sys_write),    SC(sys_openat),      SC(sys_fstatat),   SC(sys_lseek),
    SC(sys_mmap),      SC(sys_mprotect), SC(sys_munmap),      SC(sys_ioctl),     SC(sys_pread),
    SC(sys_pwrite),    SC(sys_execve),   SC(sys_sigprocmask), SC(sys_sigaction), SC(sys_kill),
    SC(sys_sigreturn), SC(sys_poll),     SC(sys_chdir),       SC(sys_getcwd),
};

void* leTable[] = {SC(le_log),           SC(le_boot_timer), SC(le_handle_close), SC(le_handle_dup),
                   SC(le_futex_wait),    SC(le_futex_wake), SC(le_set_user_tcb), SC(le_create_process),
                   SC(le_create_thread), SC(le_nanosleep)};

void** subsystems[2] = {sysTable, leTable};

long syscallCounts[2] = {SYS_SYSCALLS, LE_SYSCALLS};

extern "C" void syscall_handler(RegisterContext* r) {
    Thread* th = Thread::current();

    uint64_t num = SC_NUM(r) & 0xffff;
    uint64_t ss = SC_NUM(r) >> 16;

    if (ss >= 2) {
        Log::Warning("Invalid syscall %x", num);

        r->rax = ENOSYS;
        return;
    }

    if (num >= syscallCounts[ss]) {
        Log::Warning("Invalid syscall %x", num);

        r->rax = ENOSYS;
        return;
    }

    asm("cli");

    // If the thread is zombified, do NOT take the kernel lock
    // so that the thread can be killed
    while (th->state == ThreadStateZombie) {
        asm("sti");
        Scheduler::Yield();
    }

    while (acquireTestLock(&th->kernelLock)) {
        asm("sti");
        Scheduler::Yield();

        asm("cli");
    }
    asm("sti");

    th->scRegisters = r;

    void* call = subsystems[ss][num];
    r->rax = ((SyscallHandler)call)(SC_ARG0(r), SC_ARG1(r), SC_ARG2(r), SC_ARG3(r), SC_ARG4(r), SC_ARG5(r));

    asm("cli");
    releaseLock(&th->kernelLock);
}
