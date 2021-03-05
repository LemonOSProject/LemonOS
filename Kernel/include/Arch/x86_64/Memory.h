#pragma once

#include <PhysicalAllocator.h>
#include <Paging.h>
#include <Liballoc.h>

typedef struct {
    uint64_t base;
    uint64_t pageCount;
} mem_region_t;