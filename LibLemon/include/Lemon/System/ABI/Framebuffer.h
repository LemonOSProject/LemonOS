#pragma once

typedef struct FBInfo{
    uint32_t width; // Resolution width
    uint32_t height; // Resolution height
    uint16_t bpp; // Resolution depth/bits per pixel

    uint32_t pitch; // Video mode pitch
} __attribute__((packed)) fb_info_t;