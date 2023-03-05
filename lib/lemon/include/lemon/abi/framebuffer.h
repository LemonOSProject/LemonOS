#pragma once

enum {
    LeIoctlGetFramebuffferInfo = 0x4001
};

typedef struct FramebufferInfo {
    uint32_t width;  // Resolution width
    uint32_t height; // Resolution height
    uint16_t bpp;    // Resolution depth/bits per pixel

    uint32_t pitch; // Video mode pitch
} __attribute__((packed)) framebuffer_info_t;
