#pragma once

#include "cpu.h"

#include <le/callback.h>

namespace hal {

enum class Exception {
    DivideByZero = 0,
    Debug = 1,
    NonMaskableInterrupt = 2,
    Breakpoint = 3,
    Overflow = 4,
    BoundRangeExceeded = 5,
    InvalidOpcode = 6,
    DeviceNotAvailable = 7,
    DoubleFault = 8,
    InvalidTSS = 0xa,
    SegmentNotPresent = 0xb,
    StackSegmentFault = 0xc,
    GeneralProtectionFault = 0xd,
    PageFault = 0xe,
    FloatingPointException = 0x10,
    AlignmentCheck = 0x11,
    MachineCheck = 0x12,
    SimdFloatingPointException = 0x13
};

// Vectors 0x00-0x1f are for exceptions
constexpr int MIN_IRQ_VECTOR = 0x20;
// IRQs 0-15 are for ISA (legacy) IRQs
constexpr int MIN_FREE_IRQ_VECTOR = 0x30;

constexpr int SCHEDULE_IRQ = 0xfe;

int boot_initialize_idt();
void install_irq_handler(uint8_t vector, Callback<cpu::InterruptFrame *> cb);

}
