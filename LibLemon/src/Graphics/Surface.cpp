#include <Lemon/Graphics/Surface.h>

#include <Lemon/Core/Logger.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Rect.h>

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
    int srcWidth = (region.pos.x + region.size.x) > src->width ? (src->width - region.pos.x) : region.size.x;
    int srcHeight = (region.pos.y + region.size.y) > src->height ? (src->height - region.pos.y) : region.size.y;
    int rowSize = ((offset.x + srcWidth) > width) ? width - offset.x : srcWidth;

    if (rowSize <= 0)
        return;

    uint32_t* srcBuffer = (uint32_t*)src->buffer;
    uint32_t* destBuffer = (uint32_t*)buffer;

    for (int i = 0; i < srcHeight && i < height - offset.y; i++) {
        for (int j = 0; j < srcWidth && j < width - offset.x; j++) {
            uint32_t sPixel = (srcBuffer[(i + region.pos.y) * src->width + region.pos.x + j]);
            if (!((sPixel >> 24) & 0xFF))
                continue; // Check for 0 alpha

            if (((sPixel >> 24) & 0xFF) >= 255) { // Check for full alpha
                destBuffer[((i + offset.y) * (width) + offset.x) + j] = sPixel;
            } else {
                destBuffer[((i + offset.y) * (width) + offset.x) + j] =
                    Lemon::Graphics::AlphaBlend(destBuffer[((i + offset.y) * (width) + offset.x) + j], (sPixel >> 16) & 0xFF,
                               (sPixel >> 8) & 0xFF, sPixel & 0xFF, ((sPixel >> 24) & 0xFF) * 1.0 / 255);
            }
        }
    }
}