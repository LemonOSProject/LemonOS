#include <Lemon/Graphics/Surface.h>

#include <Lemon/Graphics/Rect.h>

#include "FastMem.h"

void Surface::Blit(const Surface* src) {
    if (width == src->width) {
        memcpy_optimized(buffer, src->buffer, width * std::min(height, src->height));
        return;
    }

    int copySize = std::min(width, src->width) << 2; // Amount of bytes to copy
    int srcPitch = src->width << 2;                  // Bytes needed to get to next line in source surface
    int effectiveHeight = std::min(height, src->height);

    for (int i = 0; i < effectiveHeight; i++) {
        memcpy_optimized(buffer + (width << 2) * i, src->buffer + srcPitch * i, copySize);
    }
}

void Surface::Blit(const Surface* src, const Vector2i& offset) {
    int xOffset = std::max(offset.x, 0) << 2;
    int yOffset = std::max(offset.y, 0);

    // Account for negative offsets
    int srcXOffset = std::max(-offset.x, 0);
    int srcYOffset = std::max(-offset.y, 0);

    int effectiveWidth = std::min(src->width - srcXOffset, width - offset.x);
    int effectiveHeight = std::min(src->height - srcYOffset, height - offset.y);
    if (effectiveWidth <= 0 || effectiveHeight <= 0) {
        return; // No pixels to copy
    }

    int copySize = effectiveWidth << 2; // Amount of bytes to copy
    int srcPitch = src->width << 2;     // Bytes needed to get to next line in source surface
    for (int i = 0; i < effectiveHeight; i++) {
        memcpy_optimized(buffer + (width << 2) * (i + yOffset) + (xOffset << 2), src->buffer + srcPitch * i + (srcXOffset << 2), copySize);
    }
}

void Surface::Blit(const Surface* src, const Vector2i& offset, const Rect& region) {
    int xBufferOffset = std::max(offset.x, 0) << 2;
    int yOffset = std::max(offset.y, 0);

    // Account for negative offsets
    int sourceXOffset = std::max(region.x, 0) + std::max(-offset.x, 0);
    int sourceYOffset = std::max(region.y, 0) + std::max(-offset.y, 0);

    // By getting the min of region.width and region.width + offset.x we account for a negative region position
    // We get the min on the adjusted region width,
    // and distance between the region start and both the source and destination surface 
    int effectiveWidth =
        std::min({std::min(region.width, region.width + offset.x), src->width - sourceXOffset, width - offset.x});
    int effectiveHeight = std::min({std::min(region.height, region.height + offset.y), src->height - sourceYOffset, height - offset.y});
    if (effectiveWidth <= 0 || effectiveHeight <= 0) {
        return; // No pixels to copy
    }

    int copySize = effectiveWidth << 2; // Amount of bytes to copy
    int srcPitch = src->width << 2;     // Bytes needed to get to next line in source surface
    for (int i = 0; i < effectiveHeight; i++) {
        memcpy_optimized(buffer + (width << 2) * (i + yOffset) + xBufferOffset, src->buffer + srcPitch * (i + region.y), copySize);
    }
}