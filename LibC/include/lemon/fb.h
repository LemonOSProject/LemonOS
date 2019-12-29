#ifndef FB_H
#define FB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct FBInfo{
    uint32_t width; // Resolution width
    uint32_t height; // Resolution height
    uint16_t bpp; // Resolution depth/bits per pixel

    uint32_t pitch; // Video mode pitch
} __attribute__((packed)) fb_info_t;

uint8_t* lemon_map_fb(struct FBInfo* fbInfo);

#ifdef __cplusplus
}
#endif

#endif