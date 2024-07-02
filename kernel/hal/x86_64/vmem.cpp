#include "vmem.h"

#include <le/lazy_constructed.h>
#include <mm/address_space.h>

#include "page_map.h"

namespace hal {

PageMap *kernel_page_map;
mm::AddressSpace *kernel_address_space;

static LazyConstructed<hal::PageMap> kernel_page_map_data;
static LazyConstructed<mm::AddressSpace> kernel_address_space_data;

static mm::MemoryRegion kernel_data_region;
static mm::MemoryRegion kernel_text_region;
static mm::MemoryRegion direct_mapping_region;

void vmem_init(uintptr_t highest_usable_physical_address) {
    kernel_page_map_data.construct();
    kernel_page_map = kernel_page_map_data.ptr();

    assert(kernel_base > direct_mapping_base);

    kernel_address_space_data.construct(kernel_page_map, direct_mapping_base);
    kernel_address_space = kernel_address_space_data.ptr();

    uintptr_t kernel_data_start = (uintptr_t)&ke_data_segment_start;
    uintptr_t kernel_data_size =
        (uintptr_t)&ke_data_segment_end - (uintptr_t)&ke_data_segment_start;
    uintptr_t kernel_text_start = (uintptr_t)&ke_text_segment_start;
    uintptr_t kernel_text_size =
        (uintptr_t)&ke_text_segment_end - (uintptr_t)&ke_text_segment_start;

    uintptr_t kernel_stack_start;

    kernel_data_region = {
        .base = kernel_data_start, .size = kernel_data_size, .prot = mm::MemoryProtection::rw()};

    kernel_text_region = {
        .base = kernel_text_start, .size = kernel_text_size, .prot = mm::MemoryProtection::rx()};

    direct_mapping_region = {.base = direct_mapping_base,
                                       .size = highest_usable_physical_address,
                                       .prot = mm::MemoryProtection::rw()};

    if (kernel_address_space->insert_region(&kernel_text_region)) {
        lemon_panic("Failed to initialize kernel memory map");
    }

    if (kernel_address_space->insert_region(&kernel_data_region)) {
        lemon_panic("Failed to initialize kernel memory map");
    }

    if (kernel_address_space->insert_region(&direct_mapping_region)) {
        lemon_panic("Failed to initialize kernel memory map");
    }

    kernel_page_map->page_range_map(
        kernel_data_region.base, kernel_data_region.base - kernel_base + kernel_phys_base,
        ARCH_X86_64_PAGE_WRITE | ARCH_X86_64_PAGE_PRESENT, kernel_data_region.size >> PAGE_BITS_4K);

    kernel_page_map->page_range_map(kernel_text_region.base,
                                   kernel_text_region.base - kernel_base + kernel_phys_base,
                                   ARCH_X86_64_PAGE_PRESENT, kernel_text_region.size >> PAGE_BITS_4K);

    kernel_page_map->page_range_map(direct_mapping_region.base, 0,
                                   ARCH_X86_64_PAGE_WRITE | ARCH_X86_64_PAGE_PRESENT,
                                   direct_mapping_region.size >> PAGE_BITS_4K);

    log_info("switching to own page tables!");
    asm volatile("mov %%rax, %%cr3" ::"a"(kernel_page_map->get_pml4()));

    // Print the address map
    log_info("Address map:");
    for (auto *region = kernel_address_space->get_first_region(); region;
         region = region->get_next()) {
        log_info("    {:x} - {:x} ({} KB) - {:x}", region->base, region->end(),
                 region->size / 1024, region->prot.prot);
    }
}

void *create_io_mapping(uintptr_t base, size_t len, const mm::MemoryProtection prot, uint64_t flags) {
    mm::MemoryRegion *region = new mm::MemoryRegion;
    if (!region) {
        return nullptr;
    }

    // By default create I/O mappings as UC
    if (flags == 0) {
        flags = ARCH_X86_64_PAGE_CACHE_DISABLE;
    }

    region->arch_flags = flags;

    // TODO: existing regions in the list overlap with the stack???!???
    if(kernel_address_space->allocate_range_for_region(region, len, prot)) {
        log_error("Failed to create I/O mapping for {}", base);
        return 0;
    }

    uint64_t page_flags = get_page_flags_for_prot(prot) | region->arch_flags;
    log_info("page flags: {:x}, prot: {:x}, arch_flags: {:x}", page_flags, get_page_flags_for_prot(prot), flags);

    kernel_page_map->page_range_map(region->base, base, page_flags, NUM_PAGES_4K(len));

    return (void *)region->base;
}

void destroy_io_mapping(uintptr_t base, size_t len, mm::MemoryProtection prot) {

}

}
