#pragma once

#include <Compiler.h>
#include <MM/KMalloc.h>
#include <Paging.h>
#include <PhysicalAllocator.h>

typedef struct {
    uint64_t base;
    uint64_t pageCount;
} mem_region_t;

template<typename T>
void KernelAllocateMappedBlock(uintptr_t* phys, T** virt) {
    *phys = Memory::AllocatePhysicalMemoryBlock();
    *virt = (T*)Memory::KernelAllocate4KPages(1);

    Memory::KernelMapVirtualMemory4K(*phys, (uintptr_t)(*virt), 1);
}

template<typename T>
void KernelAllocateMappedBlocks(uintptr_t* phys, T** virt, unsigned amount) {
    *virt = (T*)Memory::KernelAllocate4KPages(amount);
    
    for(unsigned i = 0; i < amount; i++) {
        phys[i] = Memory::AllocatePhysicalMemoryBlock();
        Memory::KernelMapVirtualMemory4K(phys[i], (uintptr_t)(*virt), 1);
    }
}
