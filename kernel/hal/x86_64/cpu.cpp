#include "cpu.h"

#include <le/list.h>

#include <stdint.h>

#include "acpi/madt.h"

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
List<CPU *> *cpus_to_initialize;

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

void register_lapic(acpi::MADTEntry *entry) {
    log_info("Local APIC, id: {}, apic_id: {}, flags: {:x}", entry->lapic.apic_id, entry->lapic.apic_id, entry->lapic.flags);
    if (entry->lapic.apic_id == 0) {
        // Ignore the BSP
        return;
    }

    cpus_to_initialize = new List<CPU *>();

    if (entry->lapic.flags & acpi::MADT_LAPIC_ENABLED) {
        // APIC can be enabled!
        auto *cpu = new CPU();
        cpu->id = entry->lapic.apic_id;

        cpus_to_initialize->push_back(cpu);
    }
}

void register_apic(acpi_madt_t *apic) {
    log_info("Scanning MADT, address: {:x}, flags: {:x}", apic->local_controller_addr, apic->flags);

    uint8_t *madt_entries = apic->madt_entries;
    while (madt_entries < ((uint8_t *)apic) + apic->header.length) {
        acpi::MADTEntry *entry = (acpi::MADTEntry *)madt_entries;

        switch (entry->entry_type) {
        case acpi::MADT_LAPIC:
            register_lapic(entry);
            break;
        case acpi::MADT_IOAPIC:
            log_info("IO APIC, id: {}, address: {:x}, gsi_base: {:x}", entry->ioapic.id, entry->ioapic.address, entry->ioapic.gsi_base);
            break;
        case acpi::MADT_ISO:
            log_info("ISO, bus: {}, source: {}, gsi: {:x}, flags: {:x}", entry->iso.bus, entry->iso.source, entry->iso.gsi, entry->iso.flags);
            break;
        case acpi::MADT_NMI_SOURCE:
            log_info("NMI Source, source: {}, flags: {:x}, gsi: {}", entry->nmi_source.source, entry->nmi_source.flags, entry->nmi_source.gsi);
            break;
        case acpi::MADT_NMI:
            log_info("NMI, processor: {}, flags: {:x}, lint: {}", entry->nmi.processor, entry->nmi.flags, entry->nmi.lint);
            break;
        case acpi::MADT_LAPIC_ADDR_OVERRIDE:
            log_info("Local APIC Address Override, address: {:x}", entry->lapic_addr_override.address);
            break;
        case acpi::MADT_X2APIC:
            log_info("x2APIC, id: {}, flags: {:x}, acpi_id: {}", entry->x2apic.x2apic_id, entry->x2apic.flags, entry->x2apic.acpi_id);
            break;
        default:
            log_fatal("Unknown MADT entry type: {}", entry->entry_type);
            break;
        }

        madt_entries += entry->length;
    }
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

