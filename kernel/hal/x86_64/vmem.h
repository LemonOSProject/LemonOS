#pragma once

#include <mm/prot.h>

#include <assert.h>
#include <stddef.h>

#define PAGES_PER_TABLE 512u

#define PAGE_SIZE_4K (1ull << 12)
#define PAGE_MASK_4K (PAGE_SIZE_4K - 1ull)
#define PAGE_BITS_4K 12

#define PAGE_SIZE_2M (1ull << 21)
#define PAGE_BITS_2M 21
#define PAGE_MASK_2M ((PAGE_SIZE_2M - 1ull) & ~PAGE_MASK_4K)

#define PAGE_SIZE_1G (1ull << 30)
#define PAGE_BITS_1G 30
#define PAGE_MASK_1G ((PAGE_SIZE_1G - 1ull) & ~PAGE_MASK_2M)

#define PDPT_BITS 39
#define PDPT_SIZE (1ull << 39)
#define PDPT_MASK ((PDPT_SIZE - 1ull) & ~PAGE_MASK_1G)

#define PML4_BITS 48
#define PML4_SIZE (1ull << 48)

// Mask off the unused high bits of the address
#define CANONICAL_ADDRESS_MASK(x) ((x) & (~0xffff000000000000ull))

#define NUM_PAGES_4K(x) ((x + PAGE_SIZE_4K - 1) >> PAGE_BITS_4K)

#define PAGE_FRAME(page) ((page) & (~0x3fffull))

enum {
    ARCH_X86_64_PAGE_PRESENT = (1u << 0),
    ARCH_X86_64_PAGE_WRITE = (1u << 1),
    ARCH_X86_64_PAGE_USER = (1u << 2),
    ARCH_X86_64_PAGE_WRITETHROUGH = (1u << 3),
    ARCH_X86_64_PAGE_CACHE_DISABLE = (1u << 4),
    ARCH_X86_64_PAGE_ACCESSED = (1u << 5),
    ARCH_X86_64_PAGE_DIRTY = (1u << 6),
    ARCH_X86_64_PAGE_SIZE = (1u << 7),
    ARCH_X86_64_PAGE_GLOBAL = (1u << 8),
    // No-execute (NX) bit
    ARCH_X86_64_PAGE_NX = (1ull << 63),
};

struct ArchX86_64PageFaultError {
    enum : uint32_t {
        Present = 1,
        Write = 2,
        User = 4,
        Reserved = 8,
        Instruction = 16,
    };

    uint32_t code;

    inline bool is_present() const {
        return code & Present;
    }

    inline bool is_write() const {
        return code & Write;
    }

    inline bool is_user() const {
        return code & User;
    }

    inline bool is_reserved() const {
        return code & Reserved;
    }

    inline bool is_instruction() const {
        return code & Instruction;
    }
};

namespace mm {

class AddressSpace;

} // namespace mm

#include "logging.h"

namespace hal {

class PageMap;

extern PageMap *kernel_page_map;
extern mm::AddressSpace *kernel_address_space;

void vmem_init(uintptr_t highest_usable_physical_address);

inline uint64_t get_page_flags_for_prot(mm::MemoryProtection prot) {
    assert(prot.read);
    
    uint64_t flags = ARCH_X86_64_PAGE_PRESENT;

    if (!prot.execute) {
        flags |= ARCH_X86_64_PAGE_NX;
    }
    
    if (prot.write) {
        flags |= ARCH_X86_64_PAGE_WRITE;
    }

    return flags;
}

/**
 * @brief Creates an uncached memory mapping for I/O use
 * 
 * @param flags Flags for mapping, defaults to ARCH_X86_64_PAGE_CACHE_DISABLE
*/
void *create_io_mapping(uintptr_t base, size_t len, mm::MemoryProtection prot,
    uint64_t flags);

/**
 * @brief Unmaps a previously mapped I/O region
 * 
 * Will panic if there is no corresponding I/O region at base.
*/
void destroy_io_mapping(uintptr_t base, size_t len, mm::MemoryProtection prot);

} // namespace hal
