#pragma once

#include <stdint.h>

#define KERNEL_CS 0x8
#define KERNEL_SS 0x10
#define USER_CS 0x18
#define USER_SS 0x20
#define TSS_SELECTOR 0x28

#define ARCH_X86_64_TSS_LEN 108

#define LOCAL_APIC_BASE 0xFEE00000

namespace hal::cpu {

struct CPU {
    void *local_apic_mapping;
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

// Set the TSS segment in the GDT to point to ptr
void set_tss(void *ptr);

void disable_8259_pic();

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

inline void flush_tlb(uintptr_t addr) {
    asm volatile("invlpg %0" :: "m"(addr));
}

} // namespace hal::cpu
