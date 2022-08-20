#pragma once

#include <Compiler.h>
#include <String.h>
#include <Types.h>
#include <UserPointer.h>

#include <stdint.h>

using UserString = const char*;
typedef UserString le_str_t;

typedef int le_handle_t;

inline long get_user_string(le_str_t str, String& kernelString) {
    long len = user_strlen((const char*)str);
    if (len < 0) {
        return 1;
    }

    kernelString.Resize(len);
    if (user_memcpy(&kernelString[0], (const char*)str, len)) {
        return 1;
    }

    return 0;
}

#define get_user_string_or_fault(str)                                                                                  \
    ({                                                                                                                 \
        String kString;                                                                                                \
        if (get_user_string(str, kString)) {                                                                           \
            return EFAULT;                                                                                             \
        }                                                                                                              \
        std::move(kString);                                                                                            \
    })

using SyscallHandler = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

#define SYSCALL extern "C"

SYSCALL long le_log(le_str_t umsg);
SYSCALL long le_boot_timer();
SYSCALL long le_handle_close(le_handle_t handle);
SYSCALL long le_handle_dup(le_handle_t handle, int flags);
SYSCALL long le_futex_wait(UserPointer<int> futex, int expected, const struct timespec* time);
SYSCALL long le_futex_wake(UserPointer<int> futex);
SYSCALL long le_set_user_tcb(uintptr_t value);

SYSCALL long sys_read(le_handle_t handle, uint8_t* buf, size_t count, UserPointer<ssize_t> bytesRead);
SYSCALL long sys_write(le_handle_t handle, const uint8_t* buf, size_t count, UserPointer<ssize_t> bytesWritten);
SYSCALL long sys_openat(le_handle_t directory, le_str_t filename, int flags, int mode, UserPointer<le_handle_t> out);
SYSCALL long sys_fstatat(int fd, le_str_t path, int flags, UserPointer<struct stat> st);
SYSCALL long sys_lseek(le_handle_t handle, off_t offset, unsigned int whence, UserPointer<off_t> out);
SYSCALL long sys_mmap(void* address, size_t len, long flags, le_handle_t handle, long off,
                      UserPointer<void*> returnAddress);
SYSCALL long sys_mprotect(void* address, size_t len, int prot);
SYSCALL long sys_munmap(void* address, size_t len);
SYSCALL long sys_ioctl(le_handle_t handle, unsigned int cmd, unsigned long arg, UserPointer<int> result);
SYSCALL long sys_pread(le_handle_t handle, void* buf, size_t count, off_t pos, UserPointer<ssize_t> bytes);
SYSCALL long sys_pwrite(le_handle_t handle, const void* buf, size_t count, off_t pos, UserPointer<ssize_t> bytes);