#include "limine.h"

#include <mm/frame_table.h>

#include <video/video.h>

#include "boot_alloc.h"
#include "logging.h"
#include "serial.h"
#include "string.h"
#include "vmem.h"
#include "panic.h"
#include "cpu.h"
#include "idt.h"

#include "page_map.h"
#include "mem_layout.h"

#define BOOT_ASSERT(x) ((x) || (boot_assert_fail(#x, __FILE__, __LINE__), 0))

LIMINE_BASE_REVISION(1);

namespace hal::boot {

const uintptr_t kernel_base = 0xffffffff80000000;
uintptr_t kernel_phys_base;

struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

// Asks the bootloader to map the first 4GB into high virtual memory
struct limine_hhdm_request limine_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 1
};

struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

const char *memmap_type_strings[] = {
    "usable",
    "reserved",
    "ACPI reclaimable",
    "ACPI NVS",
    "bad memory",
    "bootloader reclaimable",
    "kernel image",
    "framebuffer"
};

Page boot_pml4[PAGES_PER_TABLE] __attribute__(( aligned(PAGE_SIZE_4K) ));
Page boot_pdpt[PAGES_PER_TABLE] __attribute__(( aligned(PAGE_SIZE_4K) ));
Page boot_page_dir[PAGES_PER_TABLE] __attribute__(( aligned(PAGE_SIZE_4K) ));

static void boot_assert_fail(const char *why, const char *file, int line) {
    log_fatal("boot_assert_fail: {} at {}:{}\r\n", why, file, line);
    asm volatile("cli");
    asm volatile("hlt");
}

void limine_init();

extern "C"
void entry() {
    hal::cpu::boot_init((void*)limine_init);

    asm volatile("cli");
    asm volatile("hlt");
}

void limine_init() {
    serial::init();

    log_info("starting lemon...\n");

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        log_info("Unsupported Limine revision!\r\n");
    }

    limine_framebuffer *fb = nullptr;
    if (framebuffer_request.response
            && framebuffer_request.response->framebuffer_count >= 1) {
        fb = framebuffer_request.response->framebuffers[0];
    }

    if (!fb) {
        log_info("No framebuffer found!\r\n");
    } else  {
        log_info("framebuffer @ {}: {}x{} {}bpp", fb->address, fb->width, fb->height, fb->bpp);

        if (fb->bpp == 32) {
            video::set_mode(fb->address, fb->width, fb->height);
            video::fill_rect(0, 0, fb->width, fb->height, 0xff558888);
        } else {
            log_info("Only 32-bit color is supported");
        }
    }

    if (!limine_hhdm_request.response) {
        log_fatal("Failed to get higher half direct mapping from bootloader!");
        return;
    }

    if (!memory_map_request.response) {
        log_fatal("Failed to get memory map from bootloader!");
        return;
    }

    if (!kernel_address_request.response) {
        log_fatal("Failed to get kernel address from bootloader!");
        return;
    }

    boot_initialize_idt();

    BOOT_ASSERT(kernel_address_request.response->virtual_base == kernel_base);
    kernel_phys_base = kernel_address_request.response->physical_base;

    limine_memmap_entry **entries = memory_map_request.response->entries;
    size_t num_entries = memory_map_request.response->entry_count;

    // Usable pages can be used right now
    size_t usable_pages = 0;
    // Bootloader reclaimable can be used after we are done with the bootloader's page tables
    size_t bootloader_reclaimable_pages = 0;

    for (size_t i = 0; i < num_entries; i++) {
        auto *entry = entries[i];
        log_info("    {:x} - {:x} ({} KB) - {}", entry->base, entry->base + entry->length, entry->length / 1024, memmap_type_strings[entry->type]);

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            BOOT_ASSERT(entry->base % PAGE_SIZE_4K == 0);
            BOOT_ASSERT((entry->base + entry->length) % PAGE_SIZE_4K == 0);
            usable_pages += entry->length >> PAGE_BITS_4K;
        } else if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            BOOT_ASSERT(entry->base % PAGE_SIZE_4K == 0);
            BOOT_ASSERT((entry->base + entry->length) % PAGE_SIZE_4K == 0);
            bootloader_reclaimable_pages += entry->length >> PAGE_BITS_4K;
        }
    }

    log_info("{} physical memory regions, {} available pages, {} yet to be reclaimed",
        entries, usable_pages, bootloader_reclaimable_pages);

    // Find out how many pages, page tables and page directories we need for the kernel image
    uintptr_t kernel_end_address = ((uintptr_t)&kernel_end);
    size_t kernel_pages = NUM_PAGES_4K(kernel_end_address - kernel_base);

    size_t kernel_page_tables =
        ((kernel_end_address & (PAGE_SIZE_2M - 1)) + (kernel_end_address - kernel_base) + PAGE_SIZE_2M - 1) >> PAGE_BITS_2M;
    
    size_t kernel_page_dirs =
        ((kernel_end_address & (PAGE_SIZE_1G - 1)) + (kernel_end_address - kernel_base) + PAGE_SIZE_1G - 1) >> PAGE_BITS_1G;

    BOOT_ASSERT(kernel_page_dirs == 1);

    size_t kernel_page_map_memory_size = kernel_page_tables * PAGE_SIZE_4K;
    
    if (boot_alloc_free_space() < kernel_page_map_memory_size) {
        lemon_panic("Kernel image too big for boot_alloc");
        __builtin_unreachable();
    }
    
    void *kernel_page_map_memory = boot_alloc(kernel_page_map_memory_size);

    // Make sure the page map memory is page aligned
    BOOT_ASSERT(((uintptr_t)kernel_page_map_memory & (PAGE_SIZE_4K - 1)) == 0);

    // Zero out the page map memory
    memset(kernel_page_map_memory, 0, kernel_page_map_memory_size);

    // Initalize kernel page map
    boot_pml4[(kernel_base & PDPT_MASK) >> PDPT_BITS] = ((uintptr_t)boot_pdpt - kernel_base)
        | ARCH_X86_64_PAGE_PRESENT;

    boot_pml4[(kernel_base & PAGE_MASK_1G) >> PAGE_BITS_1G] = ((uintptr_t)boot_page_dir - kernel_base)
        | ARCH_X86_64_PAGE_PRESENT;
    
    for (size_t i = 0; i < kernel_page_tables; i++) {
        // Each page table takes up one page in memory
        boot_page_dir[i] = ((uintptr_t)kernel_page_map_memory) + i * PAGE_SIZE_4K - kernel_base + kernel_phys_base;
        boot_page_dir[i] |= ARCH_X86_64_PAGE_PRESENT;

        // Map a whole page table, otherwise until the end of kernel
        size_t max_page = PAGES_PER_TABLE;
        if ((i + 1) * PAGES_PER_TABLE > kernel_pages) {
            max_page = kernel_pages % PAGES_PER_TABLE;
        }

        Page *current_page = (Page *)kernel_page_map_memory + (i * PAGES_PER_TABLE);
        uintptr_t page_table_base = kernel_phys_base + i * PAGES_PER_TABLE;
        for (size_t page = 0; page < max_page; page++) {
            *current_page = page_table_base + (page << PAGE_BITS_4K);
            *current_page |= ARCH_X86_64_PAGE_PRESENT;
        }
    }

    uintptr_t kernel_start_page = (kernel_base & PAGE_MASK_1G) >> PAGE_BITS_4K;
    uintptr_t data_start_page = ((uintptr_t)&ke_data_segment_start & PAGE_MASK_1G) >> PAGE_BITS_4K;
    uintptr_t data_end_page = (((uintptr_t)&ke_data_segment_end + PAGE_SIZE_4K - 1) & PAGE_MASK_1G) >> PAGE_BITS_4K;

    // Mark data pages as writable, non execute
    for (size_t i = data_start_page; i < data_end_page; i++) {
        size_t page_dir_entry = (i >> (PAGE_BITS_2M - PAGE_BITS_4K)) & (PAGES_PER_TABLE - 1);
        size_t page_table_entry = i & (PAGES_PER_TABLE - 1);

        ((Page*)kernel_page_map_memory)[i - kernel_start_page]
            |= ARCH_X86_64_PAGE_PRESENT | ARCH_X86_64_PAGE_WRITE | ARCH_X86_64_PAGE_NX;
    }

    log_info("switching page tables!\r\n");
    asm volatile("mov %%cr3, %%rax" :: "a"((uintptr_t)boot_pml4 - kernel_base));
    log_info("using new page tables!");

    uintptr_t frame_table_metadata_addr = 0;
    size_t frame_metadata_size = sizeof(mm::Frame) * (usable_pages + bootloader_reclaimable_pages);

    // Find the lowest contiguous memory region to fit our frame metadata into
    for (size_t i = 0; i < num_entries; i++) {
        auto *entry = entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        // If the base address of the region is zero,
        // move up by a page
        size_t effective_len;
        size_t addr;
        if (entry->base == 0) {
            effective_len = entry->length + PAGE_SIZE_4K;
            addr = entry->base + PAGE_SIZE_4K;
        } else {
            effective_len = entry->length;
            addr = entry->base;
        }
        
        if (effective_len < frame_metadata_size) {
            continue;
        }

        // The entries are guaranteed to be sorted by base address, low to high
        frame_table_metadata_addr = addr;
        break;
    }

    BOOT_ASSERT(frame_table_metadata_addr > 0);   
}

}
