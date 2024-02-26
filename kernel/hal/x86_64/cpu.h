#pragma once

#define KERNEL_CS 0x8
#define KERNEL_SS 0x10
#define USER_CS 0x18
#define USER_SS 0x20
#define TSS_SELECTOR 0x28

#define ARCH_X86_64_TSS_LEN 108

namespace hal::cpu {
    // Initialize the boot CPU
    void boot_init(void *entry);

    // Set the TSS segment in the GDT to point to ptr
    void set_tss(void *ptr);
}