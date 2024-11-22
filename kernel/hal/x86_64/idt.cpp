#include "idt.h"

#include "cpu.h"
#include "panic.h"

#include <le/formatter.h>

#include <stdint.h>
#include <vmem.h>

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

static void ex_unhandled_isr();

extern void(*generic_handlers[256 - 32])(void);

namespace hal {

constexpr auto IDT_FLAGS_PRESENT = 0x80;

struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr;

IDTEntry idt[256] __attribute__((aligned(16)));
Callback<cpu::InterruptFrame*> callbacks[256 - MIN_IRQ_VECTOR];

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

void unhandled_irq(cpu::InterruptFrame *frame);

void unhandled_isr(cpu::InterruptFrame *frame) {
    lemon_panic("Unhandled interrupt");
}

void handle_int_generic(cpu::InterruptFrame *frame, int vector) {
    auto &cb = callbacks[vector - MIN_IRQ_VECTOR];
    if (cb.fn) {
        return cb.fn(cb.data, frame);
    }

    log_error("Unhandled interrupt {:x}", vector);
}

int boot_initialize_idt() {
    idt_ptr.base = (uint64_t)idt;
    idt_ptr.limit = sizeof(idt) - 1;

    for (unsigned i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)&ex_unhandled_isr, KERNEL_CS, 0, true);
    }

    for (unsigned i = 32; i < 256; i++) {
        idt_set_gate(i, (uint64_t)generic_handlers[i - 32], KERNEL_CS, 0, true);
        callbacks[i - 32].fn = nullptr;
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

uint8_t allocate_irq_vector(Callback<cpu::InterruptFrame> cb) {
    return 0;
}

void install_irq_handler(uint8_t vector, Callback<cpu::InterruptFrame *> cb) {
    assert(vector >= MIN_IRQ_VECTOR);
    log_info("Installed IRQ handler for vector {}", vector);
    
    callbacks[vector - MIN_IRQ_VECTOR] = std::move(cb);
}

void divide_by_zero(cpu::InterruptFrame *frame) {
    lemon_panic("Divide by zero", frame);
}

void non_maskable_interrupt(cpu::InterruptFrame *frame) {
    lemon_panic("Non maskable interrupt", frame);
}

void invalid_opcode(cpu::InterruptFrame *frame) {
    lemon_panic("Invalid opcode", frame);
}

void double_fault(cpu::InterruptFrame *frame) {
    lemon_panic("Double fault", frame);
}

void general_protection_fault(cpu::InterruptFrame *frame) {
    lemon_panic("General protection fault", frame);
}

void page_fault(cpu::InterruptFrame *frame) {
    ArchX86_64PageFaultError err = { (uint32_t)frame->err_code };

    log_error("Page fault");

    if (err.is_present()) {
        log_error("present");
    }

    if (err.is_write()) {
        log_error("write");
    }

    if (err.is_user()) {
        log_error("user");
    }

    if (err.is_reserved()) {
        log_error("reserved");
    }

    if (err.is_instruction()) {
        log_error("instruction");
    }

    log_error("cr2: {:x}", cpu::cr2());
    log_error("rip: {:x}", frame->rip);

    lemon_panic("Page fault", frame);
}

void fpe(cpu::InterruptFrame *frame) {
    lemon_panic("Floating point exception", frame);
}

void simd_fpe(cpu::InterruptFrame *frame) {
    lemon_panic("SIMD exception", frame);
}

}

#include "asm_macros.h"

// Push a zero error code
#define INTERRUPT_PROLOGUE asm volatile("cli\npushq $0\n" \
    PUSH_INTERRUPT_FRAME \
    "mov %rsp, %rdi\n" \
    "xor %rbp, %rbp");
    
#define INTERRUPT_PROLOGUE_HAS_ERR asm volatile("cli\n" \
    PUSH_INTERRUPT_FRAME \
    "mov %rsp, %rdi\n" \
    "xor %rbp, %rbp");

#define INTERRUPT_EPILOGUE asm volatile(POP_INTERRUPT_FRAME "iretq");

#define INTERRUPT_HANDLER(name, handler) static __attribute__((naked)) void name() { \
    INTERRUPT_PROLOGUE \
    asm volatile("callq %P0" :: "i"(handler)); \
    INTERRUPT_EPILOGUE \
    } \

#define INTERRUPT_HANDLER_N(i, name, handler) static __attribute__((naked)) void name() { \
    INTERRUPT_PROLOGUE \
    asm volatile("movq %1, %%rsi; callq %P0" :: "i"(handler), "ri"(i)); \
    INTERRUPT_EPILOGUE \
    } \

#define INTERRUPT_HANDLER_HAS_ERR(name, handler) static __attribute__((naked)) void name() { \
    INTERRUPT_PROLOGUE_HAS_ERR \
    asm volatile("callq %P0" :: "i"(handler)); \
    INTERRUPT_EPILOGUE \
    } \

INTERRUPT_HANDLER(ex_unhandled_isr, hal::unhandled_isr);

INTERRUPT_HANDLER(ex_divide_by_zero, hal::divide_by_zero);
INTERRUPT_HANDLER(ex_non_maskable_interrupt, hal::non_maskable_interrupt);
INTERRUPT_HANDLER(ex_invalid_opcode, hal::invalid_opcode);
INTERRUPT_HANDLER_HAS_ERR(ex_double_fault, hal::double_fault);
INTERRUPT_HANDLER_HAS_ERR(ex_general_protection_fault, hal::general_protection_fault);
INTERRUPT_HANDLER_HAS_ERR(ex_page_fault, hal::page_fault);
INTERRUPT_HANDLER(ex_fpe, hal::fpe);
INTERRUPT_HANDLER(ex_simd_fpe, hal::simd_fpe);

INTERRUPT_HANDLER_N(32, int32_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(33, int33_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(34, int34_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(35, int35_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(36, int36_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(37, int37_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(38, int38_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(39, int39_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(40, int40_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(41, int41_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(42, int42_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(43, int43_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(44, int44_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(45, int45_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(46, int46_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(47, int47_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(48, int48_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(49, int49_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(50, int50_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(51, int51_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(52, int52_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(53, int53_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(54, int54_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(55, int55_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(56, int56_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(57, int57_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(58, int58_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(59, int59_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(60, int60_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(61, int61_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(62, int62_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(63, int63_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(64, int64_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(65, int65_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(66, int66_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(67, int67_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(68, int68_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(69, int69_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(70, int70_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(71, int71_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(72, int72_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(73, int73_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(74, int74_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(75, int75_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(76, int76_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(77, int77_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(78, int78_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(79, int79_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(80, int80_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(81, int81_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(82, int82_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(83, int83_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(84, int84_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(85, int85_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(86, int86_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(87, int87_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(88, int88_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(89, int89_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(90, int90_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(91, int91_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(92, int92_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(93, int93_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(94, int94_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(95, int95_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(96, int96_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(97, int97_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(98, int98_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(99, int99_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(100, int100_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(101, int101_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(102, int102_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(103, int103_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(104, int104_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(105, int105_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(106, int106_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(107, int107_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(108, int108_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(109, int109_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(110, int110_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(111, int111_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(112, int112_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(113, int113_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(114, int114_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(115, int115_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(116, int116_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(117, int117_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(118, int118_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(119, int119_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(120, int120_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(121, int121_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(122, int122_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(123, int123_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(124, int124_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(125, int125_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(126, int126_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(127, int127_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(128, int128_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(129, int129_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(130, int130_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(131, int131_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(132, int132_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(133, int133_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(134, int134_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(135, int135_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(136, int136_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(137, int137_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(138, int138_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(139, int139_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(140, int140_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(141, int141_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(142, int142_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(143, int143_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(144, int144_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(145, int145_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(146, int146_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(147, int147_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(148, int148_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(149, int149_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(150, int150_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(151, int151_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(152, int152_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(153, int153_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(154, int154_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(155, int155_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(156, int156_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(157, int157_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(158, int158_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(159, int159_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(160, int160_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(161, int161_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(162, int162_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(163, int163_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(164, int164_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(165, int165_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(166, int166_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(167, int167_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(168, int168_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(169, int169_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(170, int170_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(171, int171_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(172, int172_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(173, int173_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(174, int174_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(175, int175_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(176, int176_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(177, int177_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(178, int178_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(179, int179_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(180, int180_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(181, int181_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(182, int182_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(183, int183_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(184, int184_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(185, int185_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(186, int186_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(187, int187_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(188, int188_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(189, int189_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(190, int190_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(191, int191_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(192, int192_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(193, int193_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(194, int194_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(195, int195_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(196, int196_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(197, int197_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(198, int198_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(199, int199_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(200, int200_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(201, int201_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(202, int202_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(203, int203_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(204, int204_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(205, int205_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(206, int206_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(207, int207_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(208, int208_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(209, int209_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(210, int210_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(211, int211_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(212, int212_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(213, int213_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(214, int214_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(215, int215_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(216, int216_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(217, int217_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(218, int218_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(219, int219_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(220, int220_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(221, int221_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(222, int222_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(223, int223_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(224, int224_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(225, int225_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(226, int226_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(227, int227_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(228, int228_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(229, int229_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(230, int230_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(231, int231_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(232, int232_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(233, int233_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(234, int234_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(235, int235_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(236, int236_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(237, int237_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(238, int238_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(239, int239_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(240, int240_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(241, int241_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(242, int242_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(243, int243_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(244, int244_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(245, int245_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(246, int246_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(247, int247_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(248, int248_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(249, int249_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(250, int250_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(251, int251_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(252, int252_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(253, int253_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(254, int254_handler, hal::handle_int_generic);
INTERRUPT_HANDLER_N(255, int255_handler, hal::handle_int_generic);

void(*generic_handlers[])(void) = {
    int32_handler,
    int33_handler,
    int34_handler,
    int35_handler,
    int36_handler,
    int37_handler,
    int38_handler,
    int39_handler,
    int40_handler,
    int41_handler,
    int42_handler,
    int43_handler,
    int44_handler,
    int45_handler,
    int46_handler,
    int47_handler,
    int48_handler,
    int49_handler,
    int50_handler,
    int51_handler,
    int52_handler,
    int53_handler,
    int54_handler,
    int55_handler,
    int56_handler,
    int57_handler,
    int58_handler,
    int59_handler,
    int60_handler,
    int61_handler,
    int62_handler,
    int63_handler,
    int64_handler,
    int65_handler,
    int66_handler,
    int67_handler,
    int68_handler,
    int69_handler,
    int70_handler,
    int71_handler,
    int72_handler,
    int73_handler,
    int74_handler,
    int75_handler,
    int76_handler,
    int77_handler,
    int78_handler,
    int79_handler,
    int80_handler,
    int81_handler,
    int82_handler,
    int83_handler,
    int84_handler,
    int85_handler,
    int86_handler,
    int87_handler,
    int88_handler,
    int89_handler,
    int90_handler,
    int91_handler,
    int92_handler,
    int93_handler,
    int94_handler,
    int95_handler,
    int96_handler,
    int97_handler,
    int98_handler,
    int99_handler,
    int100_handler,
    int101_handler,
    int102_handler,
    int103_handler,
    int104_handler,
    int105_handler,
    int106_handler,
    int107_handler,
    int108_handler,
    int109_handler,
    int110_handler,
    int111_handler,
    int112_handler,
    int113_handler,
    int114_handler,
    int115_handler,
    int116_handler,
    int117_handler,
    int118_handler,
    int119_handler,
    int120_handler,
    int121_handler,
    int122_handler,
    int123_handler,
    int124_handler,
    int125_handler,
    int126_handler,
    int127_handler,
    int128_handler,
    int129_handler,
    int130_handler,
    int131_handler,
    int132_handler,
    int133_handler,
    int134_handler,
    int135_handler,
    int136_handler,
    int137_handler,
    int138_handler,
    int139_handler,
    int140_handler,
    int141_handler,
    int142_handler,
    int143_handler,
    int144_handler,
    int145_handler,
    int146_handler,
    int147_handler,
    int148_handler,
    int149_handler,
    int150_handler,
    int151_handler,
    int152_handler,
    int153_handler,
    int154_handler,
    int155_handler,
    int156_handler,
    int157_handler,
    int158_handler,
    int159_handler,
    int160_handler,
    int161_handler,
    int162_handler,
    int163_handler,
    int164_handler,
    int165_handler,
    int166_handler,
    int167_handler,
    int168_handler,
    int169_handler,
    int170_handler,
    int171_handler,
    int172_handler,
    int173_handler,
    int174_handler,
    int175_handler,
    int176_handler,
    int177_handler,
    int178_handler,
    int179_handler,
    int180_handler,
    int181_handler,
    int182_handler,
    int183_handler,
    int184_handler,
    int185_handler,
    int186_handler,
    int187_handler,
    int188_handler,
    int189_handler,
    int190_handler,
    int191_handler,
    int192_handler,
    int193_handler,
    int194_handler,
    int195_handler,
    int196_handler,
    int197_handler,
    int198_handler,
    int199_handler,
    int200_handler,
    int201_handler,
    int202_handler,
    int203_handler,
    int204_handler,
    int205_handler,
    int206_handler,
    int207_handler,
    int208_handler,
    int209_handler,
    int210_handler,
    int211_handler,
    int212_handler,
    int213_handler,
    int214_handler,
    int215_handler,
    int216_handler,
    int217_handler,
    int218_handler,
    int219_handler,
    int220_handler,
    int221_handler,
    int222_handler,
    int223_handler,
    int224_handler,
    int225_handler,
    int226_handler,
    int227_handler,
    int228_handler,
    int229_handler,
    int230_handler,
    int231_handler,
    int232_handler,
    int233_handler,
    int234_handler,
    int235_handler,
    int236_handler,
    int237_handler,
    int238_handler,
    int239_handler,
    int240_handler,
    int241_handler,
    int242_handler,
    int243_handler,
    int244_handler,
    int245_handler,
    int246_handler,
    int247_handler,
    int248_handler,
    int249_handler,
    int250_handler,
    int251_handler,
    int252_handler,
    int253_handler,
    int254_handler,
    int255_handler
};
