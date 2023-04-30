#pragma once

#include <Assert.h>
#include <Compiler.h>
#include <Errno.h>
#include <String.h>
#include <Types.h>
#include <UserPointer.h>

#include <stdint.h>

#include <lemon/abi/signal.h>

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
        auto r = Process::current()->get_handle_as<File>(handle);                                                      \
        if (r.HasError()) {                                                                                            \
            return r.err.code;                                                                                         \
        }                                                                                                              \
        if (!r.Value()) {                                                                                              \
            return EBADF;                                                                                              \
        }                                                                                                              \
        std::move(r.Value());                                                                                          \
    })

#define KO_GET(T, handle)                                                                                              \
    ({                                                                                                                 \
        auto r = Process::current()->get_handle_as<T>(handle);                                                         \
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
        if (ptr.store(val))                                                                                            \
            return EFAULT;                                                                                             \
    })

#define SC_LOG_VERBOSE(msg, ...) ({ Log::Debug(debugLevelSyscalls, DebugLevelVerbose, msg, ##__VA_ARGS__); })

using UserString = const char*;
typedef UserString le_str_t;

typedef int le_handle_t;

inline Error get_user_string(le_str_t str, String& kernelString, size_t maxLength) {
    // TODO: If a string is placed right before kernel memory get_user_string will fail
    // since the maximum possible length will run into kernel space
    if (!is_usermode_pointer(str, 0, maxLength)) {
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

#define get_user_string_or_fault(str, maxLength)                                                                       \
    ({                                                                                                                 \
        String kString;                                                                                                \
        if (auto e = get_user_string(str, kString, maxLength); e) {                                                    \
            return e;                                                                                                  \
        }                                                                                                              \
        std::move(kString);                                                                                            \
    })

using SyscallHandler = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

#define DFN_SYSCALL(x) extern "C" void x()
#define SYSCALL extern "C"
