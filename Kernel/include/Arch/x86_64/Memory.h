#pragma once

#include <PhysicalAllocator.h>
#include <Paging.h>
#include <Liballoc.h>
#include <Compiler.h>

typedef struct {
    uint64_t base;
    uint64_t pageCount;
} mem_region_t;

ALWAYS_INLINE void* operator new(size_t, void* p){
	return p;
}