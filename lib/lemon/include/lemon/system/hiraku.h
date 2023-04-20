#pragma once

#include <lemon/abi/types.h>
#include <lemon/abi/signal.h>

#define HIRAKU_CALL [[gnu::weak]] extern "C"

HIRAKU_CALL long le_log(le_str_t msg);
HIRAKU_CALL long le_boot_timer();
HIRAKU_CALL long le_handle_close(le_handle_t handle);
HIRAKU_CALL long le_handle_dup(le_handle_t handle, int64_t flags);
HIRAKU_CALL long le_futex_wait(int* futex, int expected, const struct timespec* time);
HIRAKU_CALL long le_futex_wake(int* futex);
HIRAKU_CALL long le_set_user_tcb(void* tcb);
HIRAKU_CALL long le_create_process(le_handle_t* handle, uint64_t flags, le_str_t name);
HIRAKU_CALL long le_create_thread(le_handle_t* handle, uint64_t flags, void* entry, void* stack);
HIRAKU_CALL long le_nanosleep(long* nanos);

HIRAKU_CALL long sys_read(le_handle_t handle, uint8_t* buf, size_t count, ssize_t* bytesRead);
HIRAKU_CALL long sys_write(le_handle_t handle, const uint8_t* buf, size_t count, ssize_t* bytesWritten);
HIRAKU_CALL long sys_openat(le_handle_t directory, le_str_t filename, int flags, int mode, le_handle_t* out);
HIRAKU_CALL long sys_fstatat(le_handle_t fd, le_str_t path, int flags, struct stat* st);
HIRAKU_CALL long sys_lseek(le_handle_t handle, off_t offset, unsigned int whence, off_t* out);
HIRAKU_CALL long sys_mmap(void* address, size_t len, long flags, le_handle_t handle, long off, void* returnAddress);
HIRAKU_CALL long sys_mprotect(void* address, size_t len, int prot);
HIRAKU_CALL long sys_munmap(void* address, size_t len);
HIRAKU_CALL long sys_ioctl(le_handle_t handle, unsigned int cmd, unsigned long arg, int* result);
HIRAKU_CALL long sys_pread(le_handle_t handle, void* buf, size_t count, off_t pos, ssize_t* bytes);
HIRAKU_CALL long sys_pwrite(le_handle_t handle, const void* buf, size_t count, off_t pos, ssize_t* bytes);
HIRAKU_CALL long sys_exit(int status);
HIRAKU_CALL long sys_getpid();
HIRAKU_CALL long sys_execve(le_str_t filename, le_str_t const* argv, le_str_t const* envp);
HIRAKU_CALL long sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
HIRAKU_CALL long sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
HIRAKU_CALL long sys_kill(pid_t pid, pid_t tid, int signal);
