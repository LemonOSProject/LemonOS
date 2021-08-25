#pragma once

#include <Compiler.h>
#include <MM/KMalloc.h>
#include <Paging.h>
#include <PhysicalAllocator.h>

typedef struct {
    uint64_t base;
    uint64_t pageCount;
} mem_region_t;