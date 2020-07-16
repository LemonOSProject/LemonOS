#pragma once

#include <stdint.h>

typedef struct Surface{
	int width, height; // Self-explanatory
	uint8_t depth; // Pixel depth
	uint8_t* buffer; // Start of the buffer
} surface_t;