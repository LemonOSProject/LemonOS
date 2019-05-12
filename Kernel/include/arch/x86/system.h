#pragma once

#define KERNEL_VIRTUAL_BASE 0xC0000000

#include <stdint.h>

void outportb(uint16_t port, uint8_t value);

uint8_t inportb(uint16_t port);
uint16_t inportw(uint16_t port);

typedef struct {
	unsigned int gs,fs,es,ds;
	unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
	unsigned int int_num, err_code;
	unsigned int eip, cs, eflags, usr_esp, ss;
} regs32_t;