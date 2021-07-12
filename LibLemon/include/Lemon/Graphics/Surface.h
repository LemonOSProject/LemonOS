#pragma once

#include <stdint.h>

typedef struct Surface {
    int width = 0;
    int height = 0;            // Self-explanatory
    uint8_t depth = 32;        // Pixel depth
    uint8_t* buffer = nullptr; // Start of the buffer
} surface_t;
