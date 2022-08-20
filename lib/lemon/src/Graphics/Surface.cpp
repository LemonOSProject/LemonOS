#include <lemon/graphics/Surface.h>

#include <lemon/core/Logger.h>
#include <lemon/graphics/Graphics.h>
#include <lemon/graphics/Rect.h>

#include "FastMem.h"

void Surface::Blit(const Surface* src) {
    if (width == src->width) {
        memcpy_optimized(buffer, src->buffer, width * std::min(height, src->height));
        return;
    }

    int copySize = std::min(width, src->width); // Amount of uint32_ts to copy
    int srcPitch = src->width << 2;             // Bytes needed to get to next line in source surface
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

    int srcPitch = src->width << 2; // Bytes needed to get to next line in source surface
    for (int i = 0; i < effectiveHeight; i++) {
        memcpy_optimized(buffer + (width << 2) * (i + yOffset) + (xOffset << 2),
                         src->buffer + srcPitch * i + (srcXOffset << 2), effectiveWidth);
    }
}

void Surface::Blit(const Surface* src, const Vector2i& offset, const Rect& region) {
    int xOffset = offset.x;
    int yOffset = offset.y;

    Rect regionCopy = region;
    if (offset.x < 0) {
        regionCopy.left(regionCopy.x - offset.x);
        xOffset = 0;
    }

    if (offset.y < 0) {
        regionCopy.top(regionCopy.y - offset.y);
        yOffset = 0;
    }

    // Account for negative offsets
    int sourceXOffset = std::max(regionCopy.x, 0);
    int sourceYOffset = std::max(regionCopy.y, 0);

    // By getting the min of region.width and region.width + offset.x we account for a negative region position
    // We get the min on the adjusted region width,
    // and distance between the region start and both the source and destination surface
    int effectiveWidth = std::min(
        {std::min(regionCopy.width, regionCopy.width + regionCopy.x), src->width - sourceXOffset, width - xOffset});
    int effectiveHeight = std::min(
        {std::min(regionCopy.height, regionCopy.height + regionCopy.y), src->height - sourceYOffset, height - yOffset});
    if (effectiveWidth <= 0 || effectiveHeight <= 0) {
        return; // No pixels to copy
    }

    int srcPitch = src->width << 2; // Bytes needed to get to next line in source surface

    for (int i = 0; i < effectiveHeight; i++) {
        memcpy_optimized(buffer + (width << 2) * (i + yOffset) + (xOffset << 2),
                         src->buffer + srcPitch * (i + sourceYOffset) + (sourceXOffset << 2), effectiveWidth);
    }
}

// TODO: Fast blit in asm
void Surface::AlphaBlit(const Surface* src, const Vector2i& offset, const Rect& region) {
    int xOffset = offset.x;
    int yOffset = offset.y;

    Rect regionCopy = region;
    if (offset.x < 0) {
        regionCopy.left(regionCopy.x - offset.x);
        xOffset = 0;
    }

    if (offset.y < 0) {
        regionCopy.top(regionCopy.y - offset.y);
        yOffset = 0;
    }

    // Account for negative offsets
    int sourceXOffset = std::max(regionCopy.x, 0);
    int sourceYOffset = std::max(regionCopy.y, 0);

    // By getting the min of region.width and region.width + offset.x we account for a negative region position
    // We get the min on the adjusted region width,
    // and distance between the region start and both the source and destination surface
    int effectiveWidth = std::min(
        {std::min(regionCopy.width, regionCopy.width + regionCopy.x), src->width - sourceXOffset, width - xOffset});
    int effectiveHeight = std::min(
        {std::min(regionCopy.height, regionCopy.height + regionCopy.y), src->height - sourceYOffset, height - yOffset});
    if (effectiveWidth <= 0 || effectiveHeight <= 0) {
        return; // No pixels to copy
    }

    int srcPitch = src->width << 2; // Bytes needed to get to next line in source surface

    for (int i = 0; i < effectiveHeight; i++) {
        alphablend_optimized(
            reinterpret_cast<uint32_t*>(buffer + (width << 2) * (i + yOffset) + (xOffset << 2)),
            reinterpret_cast<uint32_t*>(src->buffer + srcPitch * (i + sourceYOffset) + (sourceXOffset << 2)),
            effectiveWidth);
    }
}