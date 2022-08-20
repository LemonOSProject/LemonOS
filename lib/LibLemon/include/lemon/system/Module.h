#pragma once

#include <lemon/syscall.h>

namespace Lemon {
inline static long LoadKernelModule(const char* path) { return syscall(SYS_LOAD_KERNEL_MODULE, path); }

inline static long UnloadKernelModule(const char* name) { return syscall(SYS_UNLOAD_KERNEL_MODULE, name); }
} // namespace Lemon
