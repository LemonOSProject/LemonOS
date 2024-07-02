#include "cpu.h"

#include <stdint.h>

#include "io_ports.h"
#include "logging.h"
#include "mem_layout.h"
#include "vmem.h"

#define NUM_GDT_ENTRIES 7

#define TSS_FLAGS (0x0000890000000000ull | ARCH_X86_64_TSS_LEN)

// Set the spurious interrupt vector register to enable the APIC
#define APIC_SIVR_DEFAULT (0xff) 
#define APIC_EOI_VALUE (0)

namespace hal::cpu {

enum {
    PIC1_IO_PORT = 0x20,
    PIC2_IO_PORT = 0xA0
};

enum class APICRegister {
    TaskPriorityRegister = 0x80,
    EndOfInterrupt = 0xb0,
    SpuriousInterruptVectorRegister = 0xf0,
};

enum class APICMode {
    XAPIC, // MMIO based
    X2APIC, // MSR based
} apic_mode = APICMode::XAPIC;

union InterruptCommandRegister {
    struct {
        uint32_t low;
        uint32_t high;
    };
    struct {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t destination_mode : 1;
        uint32_t unused : 2;
        // 0 - de-assert, 1 - assert
        uint32_t level_assert : 1;
        // 0 - Edge, 1 - Level
        uint32_t trigger_mode : 1;
        uint32_t more_unused : 2;
        uint32_t destination_shorthand : 2;
        uint32_t big_unused : 12;
        uint32_t destination_field;
    } __attribute__((packed));
};

uint64_t gdt_entries[NUM_GDT_ENTRIES] = {
    0xffff, // Null
    0x00209a0000000000, // Kernel code
    0x0000920000000000, // Kernel data
    0x00affa0000000000, // User code
    0x00cff20000000000, // User data
    TSS_FLAGS, // TSS
    0x0, // TSS high
};

struct {
    uint16_t size;
    uint64_t ptr;
} __attribute__((packed)) gdt_ptr;

CPU cpu0;

inline uint32_t *apic_reg(uintptr_t register_offset) {
    return (uint32_t*)((uintptr_t)cpu0.local_apic_mapping + register_offset);
}

static void apic_write(APICRegister reg, uint32_t value) {
    *apic_reg((uint64_t)reg) = value;
}

static uint32_t apic_read(APICRegister reg) {
    return *apic_reg((uint64_t)reg);
}

void boot_init(void *entry) {
    disable_8259_pic();

    gdt_ptr.size = sizeof(gdt_entries) - 1;
    gdt_ptr.ptr = (uintptr_t)gdt_entries;

    // Do the following:
    // 1. Load the GDT
    // 2. Push the necessary information for an IRETQ
    // - Data segment
    // - Stack pointer
    // - Flags
    // - Code segment
    // - Instruction pointer
    // 3. IRETQ - jump to 'entry'

    asm volatile("lgdt (%%rax); \
        push $0x10; \
        push %%rbp; \
        pushf; \
        push $0x8; \
        push %0; \
        iretq; \
        " :: "m"(entry), "a"(&gdt_ptr) : "memory");
}

void local_apic_init() {
    auto apic_mapping = create_io_mapping(LOCAL_APIC_BASE, PAGE_SIZE_4K, mm::MemoryProtection::rw(),
        ARCH_X86_64_PAGE_CACHE_DISABLE);

    if (!apic_mapping) {
        log_fatal("Failed to map APIC");
    }

    log_info("apic mapping: {:x}", (uintptr_t)apic_mapping);

    cpu0.local_apic_mapping = apic_mapping;

    // Enable the local APIC
    apic_write(APICRegister::SpuriousInterruptVectorRegister, APIC_SIVR_DEFAULT);
}

void local_apic_eoi() {
    apic_write(APICRegister::EndOfInterrupt, APIC_EOI_VALUE);
}

void set_tss(void *ptr) {
    gdt_entries[NUM_GDT_ENTRIES - 2] = TSS_FLAGS | (uintptr_t)ptr;
    gdt_entries[NUM_GDT_ENTRIES - 1] = (uintptr_t)ptr >> 32;
}

void disable_8259_pic() {
    // Remap the PICs to interrupts 0xf9-0xff,
    // these should only get triggered if there is a spurious interrupt
    // Shouldn't even happen on modern hardware but oh well
    io::out8(PIC1_IO_PORT, 0x11);
    io::out8(PIC2_IO_PORT, 0x11);
    io::out8(PIC1_IO_PORT + 1, 0xf9);
    io::out8(PIC2_IO_PORT + 1, 0xf9);
    io::out8(PIC1_IO_PORT + 1, 4);
    io::out8(PIC2_IO_PORT + 1, 2);

    // Mask all interrupts
    io::out8(PIC1_IO_PORT + 1, 0xff);
    io::out8(PIC2_IO_PORT + 1, 0xff);
}

}

