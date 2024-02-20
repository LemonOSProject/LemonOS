#include "boot_alloc.h"

#include <logging.h>
#include <stdint.h>

#define BOOT_MEMORY_POOL_SIZE 0x20000

// Static 128KB memory for boot time allocations
static uint8_t boot_memory_pool[BOOT_MEMORY_POOL_SIZE] __attribute__((aligned(4096)));
static constexpr uint8_t *boot_memory_pool_top = boot_memory_pool + BOOT_MEMORY_POOL_SIZE;

namespace hal::boot {

static uint8_t *next_alloc = boot_memory_pool;

void *boot_alloc(size_t size) {
    if (next_alloc + size > boot_memory_pool_top) {
        log_fatal("boot_alloc out of memory");
        return nullptr;
    }

    uint8_t *alloc = next_alloc;

    next_alloc += size;

    return alloc;
}

void boot_free(void *p) {
    (void)p;
    
    log_info("boot_free unimplemented");
}

uintptr_t boot_alloc_free_space() {
    return boot_memory_pool_top - next_alloc;
}

}
