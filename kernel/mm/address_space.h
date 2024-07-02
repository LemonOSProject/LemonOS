#pragma once

#include <le/intrusive_list.h>
#include <mm/prot.h>

#include <page_map.h>

#include <stddef.h>
#include <stdint.h>

namespace mm {

struct MemoryRegion : public IntrusiveListNode<MemoryRegion> {
    uintptr_t base;
    size_t size;
    uint64_t arch_flags;

    inline uintptr_t end() const {
        return base + size;
    }

    MemoryProtection prot;
};

class AddressSpace {
public:
    inline AddressSpace(hal::PageMap *page_map, uintptr_t base_address)
        : m_page_map(page_map), m_base(base_address){};

    /**
     * @brief Insert region into the region map,
     * Returns 0 on success
     *
     * @param region
     * @return int
     */
    int insert_region(MemoryRegion *region);

    /**
     * @brief Allocates a range for a region and inserts it in the map
     *
     * @param region Memory for the region, pointer must last the lifetime of the RegionMap
     * @param size Size of the region
     * @param prot Protection level
     * @return int
     */
    int allocate_range_for_region(MemoryRegion *region, size_t size, MemoryProtection prot);

    /**
     * @brief Creates a new MappedRegion and inserts it in the region map
     *
     * @param base If non-zero, address to create the region at
     * @param size Size of region to crate
     * @param prot Memory protection for the region
     */
    int create_region(uintptr_t base, size_t size, MemoryProtection prot);

    MemoryRegion *get_first_region() {
        return m_regions;
    }
private:
    class hal::PageMap *m_page_map = nullptr;
    MemoryRegion *m_regions = nullptr;

    uintptr_t m_base;
};

} // namespace mm
