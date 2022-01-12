#pragma once

#include <CPU.h>
#include <stdint.h>

#define IRQ0 32

#define IPI_HALT 0xFE
#define IPI_SCHEDULE 0xFD

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_med;
    uint32_t base_high;
    uint32_t null;
} __attribute__((packed)) idt_entry_t;

typedef void (*isr_t)(void*, RegisterContext*);

extern "C" void idt_flush();

namespace IDT {
void Initialize();
void RegisterInterruptHandler(uint8_t interrupt, isr_t handler, void* data = nullptr);

void DisablePIC();

uint8_t ReserveUnusedInterrupt();
} // namespace IDT
