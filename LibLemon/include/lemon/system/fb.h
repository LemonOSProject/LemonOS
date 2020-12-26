#pragma once 

#include <stdint.h>

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

typedef struct FBInfo{
    uint32_t width; // Resolution width
    uint32_t height; // Resolution height
    uint16_t bpp; // Resolution depth/bits per pixel

    uint32_t pitch; // Video mode pitch
} __attribute__((packed)) fb_info_t;

volatile uint8_t* LemonMapFramebuffer(FBInfo& fbInfo);