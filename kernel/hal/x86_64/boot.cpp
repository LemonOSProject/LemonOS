#include "limine.h"

#include <mm/frame_table.h>
#include <mm/region_map.h>

#include <video/video.h>

#include "boot_alloc.h"
#include "cpu.h"
#include "idt.h"
#include "logging.h"
#include "panic.h"
#include "serial.h"
#include "string.h"
#include "vmem.h"

#include "mem_layout.h"
#include "page_map.h"

#define BOOT_ASSERT(x) ((x) || (boot_assert_fail(#x, __FILE__, __LINE__), 0))

LIMINE_BASE_REVISION(1);

uintptr_t kernel_base = 0xffffffff80000000;
uintptr_t direct_mapping_base = 0;
uintptr_t kernel_phys_base;

void kernel_main();

namespace hal::boot {

struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST,
                                                         .revision = 0};

struct limine_memmap_request memory_map_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

// Asks the bootloader to map the first 4GB into high virtual memory
struct limine_hhdm_request limine_hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 1};

struct limine_kernel_address_request kernel_address_request = {.id = LIMINE_KERNEL_ADDRESS_REQUEST,
                                                               .revision = 0};

const char *memmap_type_strings[] = {"usable",       "reserved",   "ACPI reclaimable",
                                     "ACPI NVS",     "bad memory", "bootloader reclaimable",
                                     "kernel image", "framebuffer"};

Page boot_pml4[PAGES_PER_TABLE] __attribute__((aligned(PAGE_SIZE_4K)));
Page boot_pdpt[PAGES_PER_TABLE] __attribute__((aligned(PAGE_SIZE_4K)));
Page boot_page_dir[PAGES_PER_TABLE] __attribute__((aligned(PAGE_SIZE_4K)));

static void boot_assert_fail(const char *why, const char *file, int line) {
    log_fatal("boot_assert_fail: {} at {}:{}\r\n", why, file, line);
    asm volatile("cli");
    asm volatile("hlt");
}

void limine_init();

extern "C" void entry() {
    hal::cpu::boot_init((void *)limine_init);

    asm volatile("cli");
    asm volatile("hlt");
}

void limine_init() {
    serial::init();

    log_info("starting lemon...\n");

    boot_initialize_idt();

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        log_info("Unsupported Limine revision!\r\n");
    }

    limine_framebuffer *fb = nullptr;
    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count >= 1) {
        fb = framebuffer_request.response->framebuffers[0];
    }

    if (!fb) {
        log_info("No framebuffer found!\r\n");
    } else {
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

    BOOT_ASSERT(kernel_address_request.response->virtual_base == kernel_base);
    kernel_phys_base = kernel_address_request.response->physical_base;

    log_info("kernel loaded @ 0x{:x} (@ 0x{:x} in physical memory)",
             kernel_address_request.response->virtual_base,
             kernel_address_request.response->physical_base);

    log_info("HHDM mapping @ 0x{:x}", limine_hhdm_request.response->offset);
    direct_mapping_base = limine_hhdm_request.response->offset;

    limine_memmap_entry **entries = memory_map_request.response->entries;
    size_t num_entries = memory_map_request.response->entry_count;

    // Usable pages can be used right now
    size_t usable_pages = 0;
    // Bootloader reclaimable can be used after we are done with the bootloader's page tables
    size_t bootloader_reclaimable_pages = 0;
    uintptr_t highest_usable_physical_address = 0;

    for (size_t i = 0; i < num_entries; i++) {
        auto *entry = entries[i];
        log_info("    {:x} - {:x} ({} KB) - {}", entry->base, entry->base + entry->length,
                 entry->length / 1024, memmap_type_strings[entry->type]);

        if ((entry->type == LIMINE_MEMMAP_USABLE ||
             entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
             entry->type == LIMINE_MEMMAP_KERNEL_AND_MODULES) &&
            highest_usable_physical_address < entry->base + entry->length) {
            highest_usable_physical_address = entry->base + entry->length;
        }

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

    uintptr_t frame_table_metadata_addr = 0;
    size_t frame_metadata_size =
        sizeof(mm::Frame) * (highest_usable_physical_address >> PAGE_BITS_4K);

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
    BOOT_ASSERT(frame_table_metadata_addr < ADDRESS_4GB);

    log_info("highest physical address: {:x}, frame metadata size: {} frame table address: {}",
             highest_usable_physical_address, frame_metadata_size, frame_table_metadata_addr);

    mm::frame_metadata_phys = frame_table_metadata_addr;
    mm::frames = (mm::Frame *)(frame_table_metadata_addr + direct_mapping_base);

    mm::init();
    for (size_t i = 0; i < num_entries; i++) {
        auto *entry = entries[i];

        bool is_in_use = false;
        switch (entry->type) {
        case LIMINE_MEMMAP_USABLE:
            is_in_use = false;
            break;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
            is_in_use = true;
            break;
        default:
            continue;
        }

        // Split up the region so we don't mark the frame table as free
        if (frame_table_metadata_addr + frame_metadata_size >= entry->base &&
            frame_table_metadata_addr < entry->base + entry->length) {

            mm::populate_frames(frame_table_metadata_addr, frame_metadata_size, true);

            // Exploit the fact we use the START of a given region to store the frame table
            assert(frame_table_metadata_addr == entry->base);
            mm::populate_frames(entry->base + frame_metadata_size,
                                entry->length - frame_metadata_size, is_in_use);
        } else {
            mm::populate_frames(entry->base, entry->length, is_in_use);
        }
    }

    log_info("{} non-paged {} free frames {} physical memory regions, {} available pages, {} yet "
             "to be reclaimed",
             mm::num_non_paged_frames(), mm::num_free_frames(), entries, usable_pages,
             bootloader_reclaimable_pages);

    mm::RegionMap kernel_region_map{kernel_base};

    uintptr_t kernel_data_start = (uintptr_t)&ke_data_segment_start;
    uintptr_t kernel_data_size =
        (uintptr_t)&ke_data_segment_end - (uintptr_t)&ke_data_segment_start;
    uintptr_t kernel_text_start = (uintptr_t)&ke_text_segment_start;
    uintptr_t kernel_text_size =
        (uintptr_t)&ke_text_segment_end - (uintptr_t)&ke_text_segment_start;

    mm::MemoryRegion kernel_data = {
        .base = kernel_data_start, .size = kernel_data_size, .prot = mm::MemoryProtection::rw()};

    mm::MemoryRegion kernel_text = {
        .base = kernel_text_start, .size = kernel_text_size, .prot = mm::MemoryProtection::rx()};

    mm::MemoryRegion direct_mapping = {.base = direct_mapping_base,
                                       .size = highest_usable_physical_address,
                                       .prot = mm::MemoryProtection::rw()};

    if (kernel_region_map.insert_region(&kernel_text)) {
        lemon_panic("Failed to initialize kernel memory map");
    }

    if (kernel_region_map.insert_region(&kernel_data)) {
        lemon_panic("Failed to initialize kernel memory map");
    }

    if (kernel_region_map.insert_region(&direct_mapping)) {
        lemon_panic("Failed to initialize kernel memory map");
    }

    PageMap kernel_page_map;
    kernel_page_map.page_range_map(
        kernel_data.base, kernel_data.base - kernel_base + kernel_phys_base,
        ARCH_X86_64_PAGE_WRITE | ARCH_X86_64_PAGE_PRESENT, kernel_data.size >> PAGE_BITS_4K);

    kernel_page_map.page_range_map(kernel_text.base,
                                   kernel_text.base - kernel_base + kernel_phys_base,
                                   ARCH_X86_64_PAGE_PRESENT, kernel_text.size >> PAGE_BITS_4K);

    kernel_page_map.page_range_map(direct_mapping.base, 0,
                                   ARCH_X86_64_PAGE_WRITE | ARCH_X86_64_PAGE_PRESENT,
                                   direct_mapping.size >> PAGE_BITS_4K);

    log_info("switching to own page tables!");
    asm volatile("mov %%rax, %%cr3" ::"a"(kernel_page_map.get_pml4()));

    log_info("{} MB / {} MB RAM", mm::num_non_paged_frames() * 4 / 1024,
             (mm::num_free_frames() + mm::num_non_paged_frames()) * 4 / 1024);

    kernel_main();
}

} // namespace hal::boot
