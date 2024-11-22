#pragma once

#include <assert.h>
#include <stdint.h>

#include "acpi/tables.h"

#define KERNEL_CS 0x8
#define KERNEL_SS 0x10
#define USER_CS 0x18
#define USER_SS 0x20
#define TSS_SELECTOR 0x28

#define ARCH_X86_64_TSS_LEN 108

#define LOCAL_APIC_BASE 0xFEE00000

namespace thread {
    class Scheduler;
    struct TCB;
};

namespace hal::cpu {

static constexpr auto RFLAGS_INT_ENABLE = 0x0200ull;

struct CPU {
    CPU *self;

    uint32_t id;

    void *local_apic_mapping;

    thread::Scheduler *sched;
    thread::TCB *current_thread;
};

struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

// Initialize the boot CPU
void boot_init(void *entry);

// Initialize the local APIC, should be called on each CPU
void local_apic_init();
void local_apic_eoi();
void send_local_irq(uint8_t vector, uint32_t destination);

// Set the TSS segment in the GDT to point to ptr
void set_tss(void *ptr);

void disable_8259_pic();

// Process the MADT
void register_apic(acpi_madt_t *apic);

// Allocate a GSI (global system interrupt) from the APICs
uint32_t io_apic_allocate_gsi();

inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;

    asm volatile("wrmsr" :: "c"(msr), "a"(low), "d"(high));
}

inline uint64_t cr2() {
    volatile uint64_t v;
    asm volatile("mov %%cr2, %%rax" : "=a"(v));
    return v;
}

inline uint64_t cr3() {
    volatile uint64_t v;
    asm volatile("mov %%cr3, %%rax" : "=a"(v));
    return v;
}

inline uint64_t rsp() {
    volatile uint64_t v;
    asm volatile("mov %%rsp, %%rax" : "=a"(v));
    return v;
}

inline uint64_t rbp() {
    volatile uint64_t v;
    asm volatile("mov %%rbp, %%rax" : "=a"(v));
    return v;
}

inline void set_gs(uint64_t v) {
    wrmsr(0xC0000101, v);
}

inline void set_fs(uint64_t v) {
    wrmsr(0xC0000100, v);
}

inline uint64_t interrupt_flag() {
    volatile uint64_t flags;
    asm volatile("pushfq;"
        "pop %0;" : "=rm"(flags)::"memory", "cc");
    
    return flags & RFLAGS_INT_ENABLE;
}

inline void flush_tlb(uintptr_t addr) {
    asm volatile("invlpg %0" :: "m"(addr));
}

struct InterruptDisabler {
    InterruptDisabler() {
        disable();
    }

    InterruptDisabler(const InterruptDisabler &) = delete;

    InterruptDisabler(InterruptDisabler &&other) {
        ints_were_enabled = other.ints_were_enabled;
        other.ints_were_enabled = 0;
    }

    InterruptDisabler(bool should_disable_ints) {
        if (should_disable_ints)
            disable();
    }

    ~InterruptDisabler() {
        enable();
    }

    InterruptDisabler &operator=(const InterruptDisabler &) = delete;

    InterruptDisabler &operator=(InterruptDisabler &&other) {
        ints_were_enabled = other.ints_were_enabled;
        other.ints_were_enabled = 0;
        return *this;
    }

    void disable() {
        ints_were_enabled = interrupt_flag();
        asm volatile("cli");
    }

    void enable() {
        if (ints_were_enabled) {
            asm volatile("sti");
        }
    }

    int ints_were_enabled = 0;
};

inline CPU *current() {
    // Ensure the CPU struct is only accessed when interrupts are disabled
    assert(!interrupt_flag());

    CPU *cpu;
    asm volatile("mov %%gs:0, %0" : "=r"(cpu));
    return cpu;
}

} // namespace hal::cpu
