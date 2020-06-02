#pragma once

#include <stdint.h>

typedef struct Surface{
	int x, y, width, height, pitch; // Self-explanatory
	uint8_t depth; // Pixel depth
	uint8_t* buffer; // Start of the buffer
	uint8_t linePadding;
} surface_t;