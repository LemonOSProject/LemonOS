#pragma once

#include <panic.h>
#include <stdint.h>
#include <string.h>

#include <mm/frame_table.h>

#include "logging.h"
#include "vmem.h"

namespace hal {

using Page = uint64_t;

inline uintptr_t arch_page_address(Page page) {
    return page & PAGE_MASK_4K;
}

class PageMap {
public:
    static PageMap *create();
    static PageMap *create(PageMap *pm);

    void ensure_page_table_at(uintptr_t base, size_t num_pages);

    template <int level, int size, int level_shift>
    static void ensure_array_for_range(void **array, size_t start_index, size_t end_index) {
        // If this level of the array is NULL, allocate and zero it
        if (!(*array)) {
            frame_ptr_t frame = mm::alloc_frame();
            assert(frame);

            *array = frame->virtual_mapping();

            assert(*array);

            memset(*array, 0, sizeof(uintptr_t) * size);
        }

        if constexpr (level > 1) {
            void **new_array = (void **)(*array);

            // Find the indcies for our new subarray
            size_t new_array_index = start_index >> (level_shift * (level - 1));
            size_t new_array_end_index = end_index >> (level_shift * (level - 1));

            assert(new_array_end_index < size);

            size_t step = 1ull << (level_shift * (level - 1));

            while (new_array_index <= new_array_end_index) {
                // We need to update the start and end indices to match the region of
                // the subarray we are trying to allocate
                size_t new_end_index = end_index;

                // Calculate the amount of subincidies we skip over
                // as we go to the next array index
                size_t actual_step = (step - start_index % step);
                if (start_index + actual_step < end_index) {
                    new_end_index = start_index + (actual_step - 1);
                }

                ensure_array_for_range<level - 1, size, level_shift>(
                    new_array + (new_array_index % size), start_index & (step - 1),
                    new_end_index & (step - 1));

                start_index += actual_step;
                assert(start_index % step == 0);

                new_array_index++;
            }
        }
    }

    // Sets the protection flags and physical address of a range of pages
    void page_range_map(uintptr_t base, uintptr_t physical_base, uint64_t prot, size_t num_pages);

    uintptr_t get_pml4() const { return FRAME_FOR_VADDR(m_pml4)->get_address(); }

private:
    // Sets the protection flags on a range of pages
    void page_range_protect(uintptr_t base, uint64_t prot, size_t num_pages);

    Page *m_pml4 = nullptr;
    Page **m_pdpt_entries = nullptr;
    Page ***m_page_dir_entries = nullptr;
    Page ****m_page_table_entries = nullptr;
};

} // namespace hal
