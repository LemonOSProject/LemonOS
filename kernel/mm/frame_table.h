#pragma once

#include <stdint.h>
#include <stddef.h>

#include <mem_layout.h>
#include <vmem.h>

#define FRAME_FOR_VADDR(addr) ((((uintptr_t)(addr) - direct_mapping_base) >> PAGE_BITS_4K) + ::mm::frames)

namespace mm {

extern struct Frame *frames;

struct Frame final { 
    uint64_t next : 30;
    uint64_t prev : 30;

    // 3 bits to store frame list entries
    uint64_t list : 3;
    uint64_t unused : 1;
    
    inline uint64_t get_address() const {
        return (this - frames) << PAGE_BITS_4K;
    }

    inline void *virtual_mapping() const {
        return (void*)(get_address() + direct_mapping_base);
    }
} __attribute__((packed));

// Make sure the size of the frame struct is a power of 2
static_assert((sizeof(Frame) & (sizeof(Frame) - 1)) == 0);

struct PhysicalMemoryRegion {
    uintptr_t base;
    size_t frames;

    uint64_t *bitmap;
};
extern uintptr_t frame_metadata_phys;

void init();

/**
 * @brief Adds the frames corresponding to the address range to the free list,
 * unless in_use is true, where the frame get added to the non_paged list
 * 
 * @param base 
 * @param len 
 * @param in_use
 */
void populate_frames(uintptr_t base, size_t len, bool in_use);

Frame *alloc_frame();
void free_frame(Frame *frame);
uintptr_t get_frame_address(uint64_t frame);

/**
 * @brief Get the number of frames in the free list
 * 
 * @return size_t 
 */
size_t num_free_frames();

size_t num_non_paged_frames();

}

typedef mm::Frame *frame_ptr_t;
