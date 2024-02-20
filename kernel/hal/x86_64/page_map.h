#pragma once

#include <stdint.h>

#include <mm/frame_table.h>

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

#define NUM_PAGES_4K(x) ((x + PAGE_SIZE_4K - 1) >> PAGE_BITS_4K)

namespace hal {

using Page = uint64_t;

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
    ARCH_X86_64_PAGE_NX = (1ull << 64),
};

inline uintptr_t arch_page_address(Page page) {
    return page & PAGE_MASK_4K;
}

class PageMap {
public:
    static PageMap *create();
    static PageMap *create(PageMap *pm, Page *pdpt_entries, Page **page_dir_entries,
        uintptr_t* page_dirs_phys, Page ***page_table_entries, uintptr_t **page_tables_phys);

    // Sets the protection flags on a range of pages
    void page_range_protect(uintptr_t base, uint64_t prot, size_t num_pages);

    // Sets the protection flags and physical address of a range of pages
    void page_range_map(uintptr_t base, uintptr_t *addresses, uint64_t prot, size_t num_pages);

private:
    void ensure_page_table_at(uintptr_t base, size_t num_pages);

    Page *m_pdpt_entries;

    Page **m_page_dir_entries;
    uintptr_t *m_page_dirs_phys;

    Page ***m_page_table_entries;
    uintptr_t **m_page_tables_phys;
};

}
