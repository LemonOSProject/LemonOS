#pragma once

#include <stdint.h>

#include <Lemon/Graphics/Vector.h>

typedef struct Surface {
    int width = 0;
    int height = 0;            // Self-explanatory
    uint8_t depth = 32;        // Pixel depth
    uint8_t* buffer = nullptr; // Start of the buffer

    void Blit(const Surface* src);
    void Blit(const Surface* src, const Vector2i& offset);
    void Blit(const Surface* src, const Vector2i& offset, const struct Rect& region);
} surface_t;
