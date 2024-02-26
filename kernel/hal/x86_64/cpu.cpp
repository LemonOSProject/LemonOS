#include "cpu.h"

#include <stdint.h>

#define NUM_GDT_ENTRIES 7

#define TSS_FLAGS (0x0000890000000000ull | ARCH_X86_64_TSS_LEN)

namespace hal::cpu {

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

void boot_init(void *entry) {
    gdt_ptr.size = sizeof(gdt_entries) - 1;
    gdt_ptr.ptr = (uintptr_t)gdt_entries;

    asm volatile("lgdt (%%rax); \
        push $0x10; \
        push %%rbp; \
        pushf; \
        push $0x8; \
        push %0; \
        iretq; \
        " :: "m"(entry), "a"(&gdt_ptr) : "memory");
}

void set_tss(void *ptr) {
    gdt_entries[NUM_GDT_ENTRIES - 2] = TSS_FLAGS | (uintptr_t)ptr;
    gdt_entries[NUM_GDT_ENTRIES - 1] = (uintptr_t)ptr >> 32;
}

}

