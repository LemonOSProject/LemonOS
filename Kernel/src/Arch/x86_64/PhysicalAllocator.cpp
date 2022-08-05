#include <PhysicalAllocator.h>

#include <CPU.h>
#include <CString.h>
#include <Lock.h>
#include <Logging.h>
#include <Paging.h>
#include <Panic.h>
#include <Serial.h>

namespace Memory {
uint32_t physicalMemoryBitmap[PHYSALLOC_BITMAP_SIZE_DWORDS];

uint64_t usedPhysicalBlocks = PHYSALLOC_BITMAP_SIZE_DWORDS * 32;
uint64_t maxPhysicalBlocks = 0;

uint64_t nextChunk = 1;

lock_t allocatorLock = 0;

// Initialize the physical page allocator
void InitializePhysicalAllocator(memory_info_t* mem_info) {
    memset(physicalMemoryBitmap, 0xFFFFFFFF, PHYSALLOC_BITMAP_SIZE_DWORDS * sizeof(uint32_t));

    maxPhysicalBlocks = PHYSALLOC_BITMAP_SIZE_DWORDS * 32;
    usedPhysicalBlocks = maxPhysicalBlocks;
}

// Sets a bit in the physical memory bitmap
__attribute__((always_inline)) inline void SetBit(uint64_t bit) { physicalMemoryBitmap[bit >> 5] |= (1 << (bit & 31)); }

// Clears a bit in the physical memory bitmap
__attribute__((always_inline)) inline void ClearBit(uint64_t bit) {
    physicalMemoryBitmap[bit >> 5] &= (~(1 << (bit & 31)));
}

// Tests a bit in the physical memory bitmap
__attribute__((always_inline)) inline bool TestBit(uint64_t bit) {
    return physicalMemoryBitmap[bit >> 5] & (1 << (bit & 31));
}

// Finds the first free block in physical memory
uint64_t GetFirstFreeMemoryBlock() {
    if (nextChunk == 0) {
        nextChunk = 1;
    }

    for (uint32_t i = nextChunk; i < (maxPhysicalBlocks >> 5); i++) {
        if (physicalMemoryBitmap[i] != 0xffffffff) // If all 32 bits at the index are used then ignore them
            for (uint32_t j = 0; j < 32; j++) {    // Test each bit in the dword
                if (!(physicalMemoryBitmap[i] & (1 << j))) {
                    nextChunk = i;

#ifdef KERNEL_DEBUG
                    uint64_t value = (i << 5) + j;
                    assert(value > 0);

                    return value; // Make sure we don't return zero
#else
                    return (i << 5) + j;
#endif
                }
            }
    }

    for (uint32_t i = 1; i < (maxPhysicalBlocks >> 5); i++) { // Retry searching every chunk
        if (physicalMemoryBitmap[i] != 0xffffffff)            // If all 32 bits at the index are used then ignore them
            for (uint32_t j = 0; j < 32; j++) {               // Test each bit in the dword
                if (!(physicalMemoryBitmap[i] & (1 << j))) {
                    nextChunk = i;

#ifdef KERNEL_DEBUG
                    uint64_t value = (i << 5) + j;
                    assert(value > 0);

                    return value; // Make sure we don't return zero
#else
                    return (i << 5) + j;
#endif
                }
            }
    }

    // The first block is always reserved
    return 0;
}

// Marks a region in physical memory as being used
void MarkMemoryRegionUsed(uint64_t base, size_t size) {
    for (uint32_t blocks = (size + (PHYSALLOC_BLOCK_SIZE - 1)) / PHYSALLOC_BLOCK_SIZE,
                  align = base / PHYSALLOC_BLOCK_SIZE;
         blocks > 0; blocks--, usedPhysicalBlocks++)
        SetBit(align++);
}

// Marks a region in physical memory as being free
void MarkMemoryRegionFree(uint64_t base, size_t size) {
    for (uint32_t blocks = (size + (PHYSALLOC_BLOCK_SIZE - 1)) / PHYSALLOC_BLOCK_SIZE,
                  align = base / PHYSALLOC_BLOCK_SIZE;
         blocks > 0; blocks--, usedPhysicalBlocks--)
        ClearBit(align++);
}

// Allocates a block of physical memory
uint64_t AllocatePhysicalMemoryBlock() {
    ScopedSpinLock<true> lock(allocatorLock);

    uint64_t index = GetFirstFreeMemoryBlock();
    if (!index) {
        asm("cli");
        Log::Error("Out of memory!");
        KernelPanic("Out of memory!");
        for (;;)
            ;
    }

    SetBit(index);
    usedPhysicalBlocks++;

    return index << PHYSALLOC_BLOCK_SHIFT;
}

// Allocates a block of 2MB physical memory
/*uint64_t AllocateLargePhysicalMemoryBlock() {
    uint64_t index = GetFirstFreeMemoryBlock();
    uint64_t blockCount = 0x200000
    uint64_t counter = 0;
    while(counter < blockCount){
        if(bit_test(index)){
            index++;
            counter = 0;
            continue;
        }

        index++;
        counter++;
    }

    if (index == 0)
        return 0;

    uint64_t addr = index * PHYSALLOC_BLOCK_SIZE;

    while(blockCount--)
        bit_set(index++);
    usedPhysicalBlocks += counter;

    return addr;
}*/

// Frees a block of physical memory
void FreePhysicalMemoryBlock(uint64_t addr) {
    uint64_t index = addr >> PHYSALLOC_BLOCK_SHIFT;
    assert(index); // If memory < 4096 is getting freed we have a serious problem

    uint64_t chunk = index >> 5;

    physicalMemoryBitmap[chunk] = physicalMemoryBitmap[chunk] & (~(1U << (index & 31)));
    usedPhysicalBlocks--;

    if (chunk < nextChunk) {
        nextChunk = chunk;
    }
}

// Frees a block of physical memory
void FreeLargePhysicalMemoryBlock(uint64_t addr) {
    uint64_t index = addr / PHYSALLOC_BLOCK_SIZE;
    uint64_t blockCount = 0x200000 /* 2MB */ / PHYSALLOC_BLOCK_SIZE;
    usedPhysicalBlocks -= blockCount;
    while (blockCount--)
        ClearBit(index++);
}
} // namespace Memory