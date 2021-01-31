#pragma once

//#define KERNEL_VIRTUAL_BASE 0xC0000000

#include <stdint.h>

extern "C"{
    void outportb(uint16_t port, uint8_t value);
    void outportw(uint16_t port, uint16_t value);
    void outportd(uint16_t port, uint32_t value);
    void outportl(uint16_t port, uint32_t value);

    uint8_t inportb(uint16_t port);
    uint16_t inportw(uint16_t port);
    uint32_t inportd(uint16_t port);
    uint32_t inportl(uint16_t port);
}

struct RegisterContext {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

typedef struct {
    uint16_t fcw; // FPU Control Word
    uint16_t fsw; // FPU Status Word
    uint8_t ftw; // FPU Tag Words
    uint8_t zero; // Literally just contains a zero
    uint16_t fop; // FPU Opcode
    uint64_t rip;
    uint64_t rdp;
    uint32_t mxcsr; // SSE Control Register
    uint32_t mxcsrMask; // SSE Control Register Mask
    uint8_t st[8][16]; // FPU Registers, Last 6 bytes reserved
    uint8_t xmm[16][16]; // XMM Registers
} __attribute__((packed)) fx_state_t;


static inline int CheckInterrupts(){
    unsigned long flags;
    asm volatile ( "pushf;"
                   "pop %%rax;"
                   : "=a"(flags) :: "cc" );
    return (flags & 0x200);
}