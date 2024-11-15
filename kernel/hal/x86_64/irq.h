#pragma once

#include <stdint.h>

#include "idt.h"

namespace hal::cpu {

struct GSI {
    uint32_t gsi;
    uint32_t vector;

    // Whether or not this is an ISA interrupt
    // overriden by the ACPI tables
    bool is_iso;
    uint32_t legacy_irq;
    
    struct IOAPIC *io_apic;

    bool is_in_use() {
        return vector != 0 || is_iso;
    }

    void redirect(uint8_t vector);
};

constexpr uint8_t LEGACY_IRQ_PIT = 0;
constexpr uint8_t TIMER_IRQ_VECTOR = MIN_IRQ_VECTOR + LEGACY_IRQ_PIT;

GSI *get_gsi(uint32_t gsi);

} // namespace hal::cpu
