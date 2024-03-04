#pragma once

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

/*
void PageMap::page_range_protect(uintptr_t base, uint64_t prot, uintptr_t num_pages) {
    ensure_page_table_at(base, num_pages);

    uintptr_t page_dir = (base >> PAGE_BITS_1G) & 511;
    uintptr_t page_table = (base >> PAGE_BITS_2M) & 511;
    uintptr_t page_index = (base >> PAGE_BITS_4K) & 511;

    uintptr_t end_addr = base + (num_pages << PAGE_BITS_4K);

    uintptr_t max_page_dir = (max_page_dir >> PAGE_BITS_1G) & 511;
    uintptr_t max_page_table = 511;

    do {
        size_t max_page_table, max_page;

        if (page_dir >= max_page_dir) {
            max_page_table = (end_addr & PAGE_MASK_1G) >> PAGE_BITS_2M;
        } else {
            max_page_table = page_table + 511;
        }

        do {
            if (max_page_table < 511 && page_table >= max_page_table) {
                max_page = (end_addr & PAGE_MASK_2M) >> PAGE_BITS_4K;
            } else {
                max_page = page_index + 511;
            }

            while (page_index <= max_page) {
                Page *pg = &m_page_table_entries[page_dir][page_table][page_index];
                *pg = (~PAGE_MASK_4K) & (*pg) | prot;

                page_index++;
            }

            page_table++;
        } while (page_table <= max_page_table);

        page_dir++;
    } while (page_dir <= max_page_dir);
}

*/

void PageMap::ensure_page_table_at(uintptr_t base, size_t num_pages) {
    base = CANONICAL_ADDRESS_MASK(base);

    ensure_array_for_range<4, PAGES_PER_TABLE, 9>((void **)&m_page_table_entries,
                                                  base >> PAGE_BITS_4K,
                                                  (base >> PAGE_BITS_4K) + (num_pages - 1));

    ensure_array_for_range<3, PAGES_PER_TABLE, 9>((void **)&m_page_dir_entries,
                                                  base >> PAGE_BITS_2M,
                                                  (base >> PAGE_BITS_2M) + ((num_pages - 1) >> 9));

    ensure_array_for_range<2, PAGES_PER_TABLE, 9>((void **)&m_pdpt_entries, base >> PAGE_BITS_1G,
                                                  (base >> PAGE_BITS_1G) +
                                                      ((num_pages - 1) >> (9 + 9)));


    log_info("done allocating");

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

    log_info("mapping {:x}-{:x} to {:x}-{:x}", base, base + (num_pages << PAGE_BITS_4K),
             physical_base, physical_base + (num_pages << PAGE_BITS_4K));

    ensure_page_table_at(base, num_pages);

    uintptr_t page_index = (base >> PAGE_BITS_4K);
    uintptr_t page_end = (base >> PAGE_BITS_4K) + num_pages;

    while (page_index < page_end) {
        Page *pg = &m_page_table_entries[(page_index >> 27) & 511][(page_index >> 18) & 511]
                                        [(page_index >> 9) & 511][page_index & 511];

        *pg = physical_base | prot;

        page_index++;
        physical_base += PAGE_SIZE_4K;
    }
}

} // namespace hal
