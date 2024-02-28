#pragma once

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

int boot_initialize_idt();

}
