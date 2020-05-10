#pragma once

#include <physicalallocator.h>
#include <paging.h>
#include <liballoc.h>

typedef struct {
    uint64_t base;
    uint64_t pageCount;
} mem_region_t;