#include <lemon/syscall.h>

#include <lemon/abi/syscall.h>
#include <lemon/abi/types.h>
#include <lemon/abi/signal.h>
#include <lemon/system/hiraku.h>

#include <lemon/abi/peb.h>

#define PEB(x)                                                                                                         \
    ({                                                                                                                 \
        typeof(ProcessEnvironmentBlock::x) v;                                                                          \
        asm volatile("movq %%gs:%c1, %0;" : "=r"(v) : "i"(offsetof(ProcessEnvironmentBlock, x)));                      \
        v;                                                                                                             \
    })

extern "C" {

long le_log(le_str_t msg) {
    return syscall(_le_log, msg);
}

long le_boot_timer() {
    // TODO: access user-kernel shared data
    return syscall(_le_boot_timer);
}

long le_futex_wait(int* futex, int expected, const struct timespec* time) {
    return syscall(_le_futex_wait, futex, expected, time);
}

long le_futex_wake(int* futex) {
    return syscall(_le_futex_wake, futex);
}

long le_set_user_tcb(void* tcb) {
    return syscall(_le_set_user_tcb, tcb);
}

long le_handle_close(le_handle_t handle) {
    return syscall(_le_handle_close, handle);
}

long le_create_process(le_handle_t* handle, uint64_t flags, le_str_t name) {
    return syscall(_le_create_process, handle, flags, name);
}

long le_create_thread(le_handle_t* handle, uint64_t flags, void* entry, void* stack) {
    return syscall(_le_create_thread, handle, flags, entry, stack);
}

long le_nanosleep(long* nanos) {
    return syscall(_le_nanosleep, nanos);
}

long sys_read(le_handle_t handle, uint8_t* buf, size_t count, ssize_t* bytesRead) {
    return syscall(_sys_read, handle, buf, count, bytesRead);
}

long sys_write(le_handle_t handle, const uint8_t* buf, size_t count, ssize_t* bytesWritten) {
    return syscall(_sys_write, handle, buf, count, bytesWritten);
}

long sys_openat(le_handle_t directory, le_str_t filename, int flags, int mode, le_handle_t* out) {
    return syscall(_sys_openat, directory, filename, flags, mode, out);
}

long sys_fstatat(le_handle_t fd, le_str_t path, int flags, struct stat* st) {
    return syscall(_sys_fstatat, fd, path, flags, st);
}

long sys_lseek(le_handle_t handle, off_t offset, unsigned int whence, off_t* out) {
    return syscall(_sys_lseek, handle, offset, whence, out);
}

long sys_mmap(void* address, size_t len, long flags, le_handle_t handle, long off, void* returnAddress) {
    return syscall(_sys_mmap, address, len, flags, handle, off, returnAddress);
}

long sys_mprotect(void* address, size_t len, int prot) {
    return syscall(_sys_mprotect, address, len, prot);
}

long sys_munmap(void* address, size_t len) {
    return syscall(_sys_munmap, address, len);
}

long sys_ioctl(le_handle_t handle, unsigned int cmd, unsigned long arg, int* result) {
    return syscall(_sys_ioctl, handle, cmd, arg, result);
}

long sys_pread(le_handle_t handle, void* buf, size_t count, off_t pos, ssize_t* bytes) {
    return syscall(_sys_pread, handle, buf, count, pos, bytes);
}

long sys_pwrite(le_handle_t handle, const void* buf, size_t count, off_t pos, ssize_t* bytes) {
    return syscall(_sys_pwrite, handle, buf, count, pos, bytes);
}

long sys_getpid() {
    return PEB(pid);
}

long sys_execve(le_str_t filepath, le_str_t const* argv, le_str_t const* envp) {
    return syscall(_sys_execve, filepath, argv, envp);
}

long sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    return syscall(_sys_sigprocmask, set, oldset);
}

long sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact) {
    return syscall(_sys_sigaction, act, oldact);
}

long sys_kill(pid_t pid, pid_t tid, int signal) {
    return syscall(_sys_kill, pid, tid, signal);
}
}
