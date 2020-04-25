#pragma once

#include <stdint.h>
#include <system.h>

#define IRQ0 32

typedef struct {
	uint16_t base_low;
	uint16_t sel;
	uint8_t null;
	uint8_t flags;
	uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) idt_ptr_t;

typedef void(*isr_t)(regs32_t*);
namespace IDT{
	void Initialize();
	void RegisterInterruptHandler(uint8_t interrupt, isr_t handler);
}