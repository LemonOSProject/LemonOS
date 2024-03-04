#pragma once

#include <stddef.h>
#include <stdint.h>

#include <le/intrusive_list.h>

namespace mm {

union MemoryProtection {
    struct {
        uint32_t read : 1;
        uint32_t write : 1;
        uint32_t execute : 1;
    };

    uint32_t prot = 0;

    static inline MemoryProtection rw() {
        return {{true, true, false}};
    }

    static inline MemoryProtection rx() {
        return {{true, false, true}};
    }

    static inline MemoryProtection none() {
        return {{false, false, false}};
    }
};

struct MemoryRegion : public IntrusiveListNode<MemoryRegion> {
    uintptr_t base;
    size_t size;

    inline uintptr_t end() const {
        return base + size;
    }

    MemoryProtection prot;
};

class RegionMap {
public:
    inline RegionMap(uintptr_t base_address) : m_base(base_address){};

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
    int allocate_region(MemoryRegion *region, size_t size, MemoryProtection prot);

private:
    MemoryRegion *m_regions = nullptr;

    uintptr_t m_base;
};

} // namespace mm
