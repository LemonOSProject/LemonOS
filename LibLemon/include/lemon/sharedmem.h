#pragma once

#include <stdint.h>
#include <sys/types.h>

#define SMEM_FLAGS_PRIVATE 1

namespace Lemon {
    uint64_t CreateSharedMemory(uint64_t size, uint64_t flags, pid_t owner, pid_t recipient);
    void* MapSharedMemory(uint64_t key);
    long DestroySharedMemory(uint64_t key);
}