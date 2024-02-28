#include "idt.h"

#include "cpu.h"
#include "panic.h"

#include <stdint.h>

struct IDTEntry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_med;
    uint32_t base_high;
    uint32_t null;
} __attribute__((packed));

static void ex_divide_by_zero();
static void ex_non_maskable_interrupt();
static void ex_invalid_opcode();
static void ex_double_fault();
static void ex_general_protection_fault();
static void ex_page_fault();
static void ex_fpe();
static void ex_simd_fpe();

namespace hal {

struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr;

IDTEntry idt[256] __attribute__((aligned(16)));

void idt_set_gate(int index, uint64_t handler, uint16_t code_segment, uint8_t ist, bool present) {
    idt[index] = {
        .base_low = (uint16_t)handler,
        .sel = code_segment,
        .ist = ist,
        .flags = (uint8_t)(present ? 0x8e : 0x0e),
        .base_med = (uint16_t)(handler >> 16),
        .base_high = (uint32_t)(handler >> 32),
        .null = 0
    };
}

int boot_initialize_idt() {
    idt_ptr.base = (uint64_t)idt;
    idt_ptr.limit = sizeof(idt) - 1;

    for (unsigned i = 0; i < 256; i++) {
        idt_set_gate(i, 0, KERNEL_CS, 0, false);
    }

    idt_set_gate((int)Exception::DivideByZero, (uint64_t)&ex_divide_by_zero, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::NonMaskableInterrupt, (uint64_t)&ex_non_maskable_interrupt, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::InvalidOpcode, (uint64_t)&ex_invalid_opcode, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::DoubleFault, (uint64_t)&ex_double_fault, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::GeneralProtectionFault, (uint64_t)&ex_general_protection_fault, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::PageFault, (uint64_t)&ex_page_fault, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::FloatingPointException, (uint64_t)&ex_fpe, KERNEL_CS, 0, true);
    idt_set_gate((int)Exception::SimdFloatingPointException, (uint64_t)&ex_simd_fpe, KERNEL_CS, 0, true);

    asm volatile("lidt %0" :: "m"(idt_ptr) : "memory");

    return 0;
}

void divide_by_zero(cpu::InterruptFrame *frame) {
    lemon_panic("Divide by zero");
}

void non_maskable_interrupt(cpu::InterruptFrame *frame) {
    lemon_panic("Non maskable interrupt");
}

void invalid_opcode(cpu::InterruptFrame *frame) {
    lemon_panic("Invalid opcode");
}

void double_fault(cpu::InterruptFrame *frame) {
    lemon_panic("Double fault");
}

void general_protection_fault(cpu::InterruptFrame *frame) {
    lemon_panic("General protection fault");
}

void page_fault(cpu::InterruptFrame *frame) {
    lemon_panic("Page fault");
}

void fpe(cpu::InterruptFrame *frame) {
    lemon_panic("Floating point exception");
}

void simd_fpe(cpu::InterruptFrame *frame) {
    lemon_panic("SIMD exception");
}

}

#define PUSH_INTERRUPT_FRAME "pushq %rax \n\
    pushq %rbx \n\
    pushq %rcx \n\
    pushq %rdx \n\
    pushq %rbp \n\
    pushq %rsi \n\
    pushq %rdi \n\
    pushq %r8 \n\
    pushq %r9 \n\
    pushq %r10 \n\
    pushq %r11 \n\
    pushq %r12 \n\
    pushq %r13 \n\
    pushq %r14 \n\
    pushq %r15 \n"

// Get rid of the error code at the end
#define POP_INTERRUPT_FRAME "popq %r15 \n\
    popq %r14 \n\
    popq %r13 \n\
    popq %r12 \n\
    popq %r11 \n\
    popq %r10 \n\
    popq %r9 \n\
    popq %r8 \n\
    popq %rdi \n\
    popq %rsi \n\
    popq %rbp \n\
    popq %rdx \n\
    popq %rcx \n\
    popq %rbx \n\
    popq %rax \n\
    addq $8, %rsp \n"

// Push a zero error code
#define INTERRUPT_PROLOGUE asm volatile("pushq $0\n" \
    PUSH_INTERRUPT_FRAME \
    "mov %rsp, %rdi\n" \
    "xor %rbp, %rbp");
    
#define INTERRUPT_PROLOGUE_HAS_ERR asm volatile(PUSH_INTERRUPT_FRAME \
    "mov %rsp, %rdi\n" \
    "xor %rbp, %rbp");

#define INTERRUPT_EPILOGUE asm volatile(POP_INTERRUPT_FRAME "iretq");

#define INTERRUPT_HANDLER(name, handler) static __attribute__((naked)) void name() { \
    INTERRUPT_PROLOGUE \
    asm volatile("callq %P0" :: "i"(handler)); \
    INTERRUPT_EPILOGUE \
    } \

#define INTERRUPT_HANDLER_HAS_ERR(name, handler) static __attribute__((naked)) void name() { \
    INTERRUPT_PROLOGUE_HAS_ERR \
    asm volatile("callq %P0" :: "i"(handler)); \
    INTERRUPT_EPILOGUE \
    } \

INTERRUPT_HANDLER(ex_divide_by_zero, hal::divide_by_zero);
INTERRUPT_HANDLER(ex_non_maskable_interrupt, hal::non_maskable_interrupt);
INTERRUPT_HANDLER(ex_invalid_opcode, hal::invalid_opcode);
INTERRUPT_HANDLER_HAS_ERR(ex_double_fault, hal::double_fault);
INTERRUPT_HANDLER_HAS_ERR(ex_general_protection_fault, hal::general_protection_fault);
INTERRUPT_HANDLER_HAS_ERR(ex_page_fault, hal::page_fault);
INTERRUPT_HANDLER(ex_fpe, hal::fpe);
INTERRUPT_HANDLER(ex_simd_fpe, hal::simd_fpe);
