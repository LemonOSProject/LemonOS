#pragma once

#include <stddef.h>

namespace hal::boot {
    [[ nodiscard ]] void *boot_alloc(size_t size);
    
    void boot_free(void *p);
    size_t boot_alloc_free_space();
}
