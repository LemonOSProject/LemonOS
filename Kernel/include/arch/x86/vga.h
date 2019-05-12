#pragma once

#include <stdint.h>

namespace VGA {
	void putc(char c, uint8_t colour = 0x0F);
	void puts(char* string, uint8_t colour = 0x0F);

	void clearscreen(uint8_t colour = 0x00);
}