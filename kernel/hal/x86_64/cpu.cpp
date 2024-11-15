#include "cpu.h"

#include <le/list.h>

#include <stdint.h>

#include "acpi/madt.h"

#include "io_ports.h"
#include "logging.h"
#include "mem_layout.h"
#include "irq.h"
#include "vmem.h"

#define NUM_GDT_ENTRIES 7

#define TSS_FLAGS (0x0000890000000000ull | ARCH_X86_64_TSS_LEN)

// Set the spurious interrupt vector register to enable the APIC
#define APIC_SIVR_DEFAULT (0xff) 
#define APIC_SIVR_APIC_ENABLE (1 << 8)
#define APIC_EOI_VALUE (0)

#define IO_APIC_RED_TABLE_ENT(x) (0x10 + 2 * x)
#define IO_APIC_RED_MASK (1 << 16)
#define IO_APIC_REG_SEL 0
#define IO_APIC_IO_WIN 0x4

namespace hal {

void unhandled_irq(cpu::InterruptFrame *frame) {
    log_error("Unhandled interrupt");
}

}

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

enum {
    IO_APIC_REG_ID = 0x0, // ID Register
    IO_APIC_REG_VER = 0x1, // Version Register
    IO_APIC_REG_ARB_ID = 0x2, // I/O APIC Arbitration ID
    IO_APIC_REG_RED_TBL = 0x10 // I/O APIC Redirection Table Start
};

struct IOAPIC {
    void *mapping;
    uintptr_t address;

    // First interrupt this apic handles
    uint32_t interrupt_base;
    // Last interrupt this apic handles
    uint32_t interrupt_end;

    void write32(uint32_t reg, uint32_t value) {
        *((uint32_t *)mapping + IO_APIC_REG_SEL) = reg;
        *((uint32_t *)mapping + IO_APIC_IO_WIN) = value;
    }

    uint32_t read32(uint32_t reg) {
        *((uint32_t *)mapping + IO_APIC_REG_SEL) = reg;
        return *(((uint32_t *)mapping) + IO_APIC_IO_WIN);
    }

    void redirect(uint32_t source, uint8_t vector, uint32_t delivery) {
        write32(IO_APIC_RED_TABLE_ENT(source), delivery | vector);
        write32(IO_APIC_RED_TABLE_ENT(source) + 0x1, 0);
    }
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
List<IOAPIC *> *io_apics;
List<GSI *> *global_irqs;

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

    if (entry->lapic.flags & acpi::MADT_LAPIC_ENABLED) {
        // APIC can be enabled!
        auto *cpu = new CPU();
        cpu->id = entry->lapic.apic_id;

        cpus_to_initialize->push_back(cpu);
    }
}

void register_io_apic(acpi::MADTEntry *entry) {
    void *io_apic_mapping = create_io_mapping(entry->ioapic.address, PAGE_SIZE_4K,
        mm::MemoryProtection::rw());

    assert(io_apic_mapping);

    auto io_apic = new IOAPIC;
    assert(io_apic);

    *io_apic = {
        .mapping = io_apic_mapping,
        .address = entry->ioapic.address,
        .interrupt_base = entry->ioapic.gsi_base,
        .interrupt_end = 0,
    }; 

    uint32_t interrupt_count = (io_apic->read32(IO_APIC_REG_VER) >> 16) & 0xff;
    io_apic->interrupt_end = interrupt_count;

    log_info("IO APIC, id: {}, address: {:x}, interrupt range: {:x} - {:x}", entry->ioapic.id,
        entry->ioapic.address, io_apic->interrupt_base, io_apic->interrupt_end);

    // Mask all GSIs
    for (auto i = io_apic->interrupt_base; i <= io_apic->interrupt_end; i++) {
        auto *gsi = new GSI {
            .gsi = i,
            .vector = 0,
            .is_iso = false,
            .legacy_irq = 0,
            .io_apic = io_apic,
        };

        global_irqs->push_back(gsi);

        io_apic->redirect(i, 0, IO_APIC_RED_MASK);
    }

    io_apics->push_back(io_apic);
}

void register_apic(acpi_madt_t *apic) {
    cpus_to_initialize = new List<CPU *>();
    io_apics = new List<IOAPIC *>();
    global_irqs = new List<GSI *>();

    log_info("Scanning MADT, address: {:x}, flags: {:x}", apic->local_controller_addr, apic->flags);

    List<acpi::MADTEntry *> isos;

    uint8_t *madt_entries = apic->madt_entries;
    while (madt_entries < ((uint8_t *)apic) + apic->header.length) {
        acpi::MADTEntry *entry = (acpi::MADTEntry *)madt_entries;

        switch (entry->entry_type) {
        case acpi::MADT_LAPIC:
            register_lapic(entry);
            break;
        case acpi::MADT_IOAPIC:
            register_io_apic(entry);
            break;
        case acpi::MADT_ISO:
            isos.push_back(entry);
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

    // Process interrupt source overrides
    for (auto *entry : isos) {
        bool found = false;
        for (auto *irq : *global_irqs) {
            if (irq->gsi == entry->iso.gsi) {
                irq->is_iso = true;
                irq->legacy_irq = entry->iso.source;

                log_info("ISO mapping legacy IRQ {:x} -> GSI {:x}", entry->iso.source, entry->iso.gsi, entry->iso.gsi);

                found = true;
            }
        }

        if (!found) {
            log_error("GSI {:x} for ISO mapping {:x} -> {:x} not found", entry->iso.gsi, entry->iso.source, entry->iso.gsi);
        }
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
    auto sivr = apic_read(APICRegister::SpuriousInterruptVectorRegister);
    apic_write(APICRegister::SpuriousInterruptVectorRegister, sivr | APIC_SIVR_DEFAULT | APIC_SIVR_APIC_ENABLE);
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

void GSI::redirect(uint8_t vector) {
    if (io_apic) {
        io_apic->redirect(gsi, vector, 0);
    }
}

GSI *get_gsi(uint32_t gsi) {
    for (auto *irq : *global_irqs) {
        if (irq->gsi == gsi) {
            return irq;
        }
    }

    return nullptr;
}

}

