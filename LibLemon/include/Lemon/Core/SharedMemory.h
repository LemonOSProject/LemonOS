#pragma once

#include <stdint.h>
#include <sys/types.h>

#define SMEM_FLAGS_PRIVATE 1
#define SMEM_FLAGS_SHARED 0

namespace Lemon {
int64_t CreateSharedMemory(uint64_t size, uint64_t flags);
volatile void* MapSharedMemory(int64_t key);
long UnmapSharedMemory(void* address, int64_t key);
long DestroySharedMemory(int64_t key);
} // namespace Lemon
