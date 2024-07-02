#include "page_map.h"

#include "boot_alloc.h"
#include "vmem.h"

#include <assert.h>
#include <new>

namespace hal {

PageMap *PageMap::create() {
    return nullptr;
}

PageMap *PageMap::create(PageMap *pm) {
    new (pm) PageMap();

    return pm;
}

void PageMap::ensure_page_table_at(uintptr_t base, size_t num_pages) {
    base = CANONICAL_ADDRESS_MASK(base);

    uint64_t base_page = base >> PAGE_BITS_4K;
    uint64_t end_page = base_page + num_pages - 1;

    ensure_array_for_range<4, PAGES_PER_TABLE, 9>((void **)&m_page_table_entries,
                                                  base_page,
                                                  end_page);

    ensure_array_for_range<3, PAGES_PER_TABLE, 9>((void **)&m_page_dir_entries,
                                                  base_page >> 9,
                                                  (end_page >> 9));

    ensure_array_for_range<2, PAGES_PER_TABLE, 9>((void **)&m_pdpt_entries, base_page >> (9 + 9),
                                                  (end_page >> (9 + 9)));

    if (!m_pml4) {
        auto frame = mm::alloc_frame();
        assert(frame);

        m_pml4 = (Page *)frame->virtual_mapping();
        memset(m_pml4, 0, PAGE_SIZE_4K);
    }

    uintptr_t end_addr = base + (num_pages << PAGE_BITS_4K) - 1;

    uintptr_t pdpt = base >> (PAGE_BITS_4K + 9 + 9 + 9);
    uintptr_t start_pdpt = pdpt;
    uintptr_t end_pdpt = end_addr >> (PAGE_BITS_4K + 9 + 9 + 9);

    while (pdpt <= end_pdpt) {
        if (!(m_pml4[pdpt] & ARCH_X86_64_PAGE_PRESENT)) {
            m_pml4[pdpt] = FRAME_FOR_VADDR(m_pdpt_entries[pdpt])->get_address() |
                           ARCH_X86_64_PAGE_PRESENT | ARCH_X86_64_PAGE_WRITE;
        }

        Page *pdpt_entries = m_pdpt_entries[pdpt];

        int32_t page_dir = PAGES_PER_TABLE - 1;

        int32_t start_page_dir = 0;
        if (pdpt == start_pdpt) {
            start_page_dir = (base >> PAGE_BITS_1G) & (PAGES_PER_TABLE - 1);
        }

        // Set end_page_dir to an unreachable value unless we are on the last PDPT
        int32_t end_page_dir = PAGES_PER_TABLE;
        if (pdpt == end_pdpt) {
            page_dir = (end_addr >> PAGE_BITS_1G) & (PAGES_PER_TABLE - 1);
            end_page_dir = page_dir;
        }

        while (page_dir >= start_page_dir) {
            if (!(pdpt_entries[page_dir] & ARCH_X86_64_PAGE_PRESENT)) {
                pdpt_entries[page_dir] =
                    FRAME_FOR_VADDR(m_page_dir_entries[pdpt][page_dir])->get_address() |
                    ARCH_X86_64_PAGE_PRESENT | ARCH_X86_64_PAGE_WRITE;
            }

            int32_t page_table = PAGES_PER_TABLE - 1;
            // If we are the last page dir, find the highest page table to map
            if (page_dir == end_page_dir) {
                page_table = (end_addr >> PAGE_BITS_2M) & (PAGES_PER_TABLE - 1);
            }
            // If we are the first page dir, find the lowest page table to map
            int32_t lowest_page_table = 0;
            if (page_dir == start_page_dir && pdpt == start_pdpt) {
                lowest_page_table = (base >> PAGE_BITS_2M) & (PAGES_PER_TABLE - 1);
            }

            Page *page_dir_entries = m_page_dir_entries[pdpt][page_dir];
            while (page_table >= lowest_page_table) {
                if (!(page_dir_entries[page_table] & ARCH_X86_64_PAGE_PRESENT)) {
                    page_dir_entries[page_table] =
                        FRAME_FOR_VADDR(m_page_table_entries[pdpt][page_dir][page_table])
                            ->get_address() |
                        ARCH_X86_64_PAGE_PRESENT | ARCH_X86_64_PAGE_WRITE;
                }

                page_table--;
            }

            page_dir--;
        }

        pdpt++;
    }
}

void PageMap::page_range_map(uintptr_t base, uintptr_t physical_base, uint64_t prot,
                             uintptr_t num_pages) {
    assert(!(base & (PAGE_SIZE_4K - 1)));
    assert(!(physical_base & (PAGE_SIZE_4K - 1)));
    
    ensure_page_table_at(base, num_pages);

    uintptr_t page_index = (base >> PAGE_BITS_4K);
    uintptr_t page_end = (base >> PAGE_BITS_4K) + num_pages;

    while (page_index < page_end) {
        Page *pg = &m_page_table_entries[(page_index >> 27) & 511][(page_index >> 18) & 511]
                                        [(page_index >> 9) & 511][page_index & 511];

        if (num_pages == 1) {
            log_info("pg: {:x}, addresses: {:x}", pg, 0xffff000000000000ul | (page_index << PAGE_BITS_4K));
        }

        *pg = physical_base | prot;

        asm volatile("invlpg (%0)" ::"r"(base));

        page_index++;

        physical_base += PAGE_SIZE_4K;
        base += PAGE_SIZE_4K;
    }
}

} // namespace hal
