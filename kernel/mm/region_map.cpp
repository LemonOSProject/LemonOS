#include "region_map.h"

#include <assert.h>
#include <logging.h>

#include <page_map.h>

namespace mm {

int RegionMap::insert_region(MemoryRegion *region) {
    if (!m_regions) {
        // If there are no regions initialize a new list
        m_regions = region->create_list();
        return 0;
    }

    auto it = m_regions;
    while (it) {
        // We have found a gap, insert our region
        if (it->base > region->end()) {

            // If this region is the head, we need to update the head!
            if (m_regions == it) {
                m_regions = it->insert_before(region);
            }

            return 0;
        }

        if (it->base <= region->base && it->end() > region->base) {
            return 1; // Overlapping regions
        }

        it = it->get_next();
    }

    m_regions->push_back(region);

    return 0;
}

int RegionMap::allocate_region(MemoryRegion *region, size_t size, MemoryProtection prot) {
    assert(!(size & (PAGE_SIZE_4K - 1)));

    if (!m_regions) {
        // If there are no regions initialize a new list
        m_regions = region->create_list();
        return 0;
    }

    auto it = m_regions;
    uintptr_t last_region_end = m_base;

    while (it) {
        // We have found a gap, insert our region
        if (it->base - last_region_end > size) {
            // If this region is the head, we need to update the head!
            if (m_regions == it) {
                region->base = last_region_end;
                region->size = size;

                m_regions = it->insert_before(region);
            }

            return 0;
        }

        last_region_end = it->end();
        it = it->get_next();
    }

    assert(last_region_end > 0);
    // Make sure we do not overflow and have enough space to fit another region at the end
    if (last_region_end + size < last_region_end) {
        log_error("Region allocation would cause overflow");
        return 1;
    }
    
    region->base = last_region_end;
    region->size = size;
    region->prot = prot;

    m_regions->push_back(region);

    return 0;
}

} // namespace mm
