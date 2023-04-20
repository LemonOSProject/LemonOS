#pragma once

#include <Assert.h>
#include <Compiler.h>
#include <Errno.h>
#include <String.h>
#include <Types.h>
#include <UserPointer.h>

#include <stdint.h>

#define SC_TRY_OR_ERROR(func)                                                                                          \
    ({                                                                                                                 \
        auto result = func;                                                                                            \
        if (result.HasError()) {                                                                                       \
            return result.err.code;                                                                                    \
        }                                                                                                              \
        std::move(result.Value());                                                                                     \
    })

#define SC_TRY(func)                                                                                                   \
    ({                                                                                                                 \
        if (Error e = func; e.code)                                                                                    \
            return e.code;                                                                                             \
    })

#define FD_GET(handle)                                                                                                 \
    ({                                                                                                                 \
        auto r = Process::current()->get_handle_as<File>(handle);                                                        \
        if (r.HasError()) {                                                                                            \
            return r.err.code;                                                                                         \
        }                                                                                                              \
        if (!r.Value()) {                                                                                              \
            return EBADF;                                                                                              \
        }                                                                                                              \
        std::move(r.Value());                                                                                          \
    })

#define KO_GET(T, handle)                                                                                                 \
    ({                                                                                                                 \
        auto r = Process::current()->get_handle_as<T>(handle);                                                        \
        if (r.HasError()) {                                                                                            \
            return r.err.code;                                                                                         \
        }                                                                                                              \
        if (!r.Value()) {                                                                                              \
            return EBADF;                                                                                              \
        }                                                                                                              \
        std::move(r.Value());                                                                                          \
    })

#define SC_USER_STORE(ptr, val)                                                                                        \
    ({                                                                                                                 \
        if (ptr.store(val))                                                                                       \
            return EFAULT;                                                                                             \
    })

#define SC_LOG_VERBOSE(msg, ...) ({ Log::Debug(debugLevelSyscalls, DebugLevelVerbose, msg, ##__VA_ARGS__); })

using UserString = const char*;
typedef UserString le_str_t;

typedef int le_handle_t;

inline Error get_user_string(le_str_t str, String& kernelString, size_t maxLength) {
    // TODO: If a string is placed right before kernel memory get_user_string will fail
    // since the maximum possible length will run into kernel space
    if(!is_usermode_pointer(str, 0, maxLength)) {
        return EFAULT;
    }

    long len = user_strnlen((const char*)str, maxLength);
    if (len < 0) {
        return EFAULT;
    }

    kernelString.Resize(len);
    if (user_memcpy(&kernelString[0], (const char*)str, len)) {
        return EFAULT;
    }

    return ERROR_NONE;
}

#define get_user_string_or_fault(str, maxLength)                                                                                  \
    ({                                                                                                                 \
        String kString;                                                                                                \
        if (auto e = get_user_string(str, kString, maxLength); e) {                                                                           \
            return e;                                                                                             \
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
SYSCALL long le_create_process(UserPointer<le_handle_t> handle, uint64_t flags, le_str_t name);
SYSCALL long le_start_process(le_handle_t handle);
SYSCALL long le_create_thread(UserPointer<le_handle_t> handle, uint64_t flags, void* entry, void* stack);
SYSCALL long le_nanosleep(UserPointer<long> nanos);

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
SYSCALL long sys_execve(le_str_t filepath, UserPointer<le_str_t> argv, UserPointer<le_str_t> envp);
