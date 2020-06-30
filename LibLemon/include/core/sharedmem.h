#pragma once

#include <stdint.h>
#include <sys/types.h>

#define SMEM_FLAGS_PRIVATE 1
#define SMEM_FLAGS_SHARED 0

namespace Lemon {
    uint64_t CreateSharedMemory(uint64_t size, uint64_t flags);
    void* MapSharedMemory(uint64_t key);
    long UnmapSharedMemory(void* address, uint64_t key);
    long DestroySharedMemory(uint64_t key);
}