#include <gfx/graphics.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>

#include <math.h>
#include <string.h>
#include <stdlib.h>

extern "C" void memcpy_sse2(void* dest, void* src, size_t count);
extern "C" void memcpy_sse2_unaligned(void* dest, void* src, size_t count);
extern "C" void memset32_sse2(void* dest, uint32_t c, uint64_t count);
extern "C" void memset64_sse2(void* dest, uint64_t c, uint64_t count);
#ifdef Lemon32
void memcpy_optimized(void* dest, void* src, size_t count) {
    //if(((size_t)dest % 0x10) || ((size_t)src % 0x10)) memcpy(dest, src, count); 

    dest = dest; //+ 0x10-start_overflow;
    count -= 0;//0x10 - start_overflow;

    size_t overflow = (count % 0x10); // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    if(((size_t)dest % 0x10) || ((size_t)src % 0x10))
        memcpy_sse2_unaligned(dest, src, size_aligned/0x10);
    else
        memcpy_sse2(dest, src, size_aligned/0x10);

    if (overflow > 0)
        memcpy(dest + size_aligned, src + size_aligned, overflow);
}

void memset32_optimized(void* dest, uint32_t c, size_t count) {
    if(((size_t)dest % 0x10)){
        while(count--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
        return;
    }

    size_t overflow = (count % 0x4); // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    memset_sse2(dest, c, size_aligned/0x4);

    while(overflow--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
}
#else
void memset32_optimized(void* dest, uint32_t c, size_t count) {
    //{//if(((size_t)dest % 0x10)){
        while(count--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
        return;
    /*}

    size_t overflow = (count % 0x4); // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    memset32_sse2(dest, c, size_aligned/0x4);

    while(overflow--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }*/
}

void memset64_optimized(void* dest, uint64_t c, size_t count) {
    if(((size_t)dest % 0x10)){
        while(count--){
            *((uint64_t*)dest) = c;
            dest+=sizeof(uint64_t);
        }
        return;
    }

    size_t overflow = (count % 0x8); // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    memset64_sse2(dest, c, size_aligned/0x8);

    while(overflow--){
            *((uint64_t*)dest) = c;
            dest+=sizeof(uint64_t);
        }
}

void memcpy_optimized(void* dest, void* src, size_t count) {
    if(((size_t)dest % 0x10) || ((size_t)src % 0x10)) {
        memcpy(dest, src, count);
        return;
    }

    size_t overflow = (count % 0x10); // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    if(((size_t)dest % 0x10) || ((size_t)src % 0x10))
        memcpy_sse2_unaligned(dest, src, size_aligned/0x10);
    else
        memcpy_sse2(dest, src, size_aligned/0x10);

    if (overflow > 0)
        memcpy(dest + size_aligned, src + size_aligned, overflow);
}
#endif

namespace Lemon::Graphics{

    surface_t* CreateFramebufferSurface(fb_info_t fbInfo, void* address){
        surface_t* surface = (surface_t*)malloc(sizeof(surface_t));
        surface->x = surface->y = 0;
        surface->width = fbInfo.width;
        surface->height = fbInfo.height;
        surface->depth = fbInfo.bpp;
        surface->pitch = fbInfo.pitch;

        surface->buffer = (uint8_t*)address;

        return surface;
    }

    // Check if a point lies inside a rectangle
    bool PointInRect(rect_t rect, vector2i_t point){
        return (point.x >= rect.pos.x && point.x < rect.pos.x + rect.size.x && point.y >= rect.pos.y && point.y < rect.pos.y + rect.size.y);
    }

    /*int floor(double num) {
        int x = (int)num;
        return num < x ? x - 1 : x;
    }*/

    void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface){
        DrawRect(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, colour, surface);
    }

    void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface){
        if(x < 0){
            width += x;
            x = 0;
        }

        if(y < 0){
            height += y;
            y = 0;
        }
        
        int _width = ((x + width) < surface->width) ? width : (surface->width - x);
        uint32_t colour_i = 0xFF000000 | (r << 16) | (g << 8) | b;
        uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
        for(int i = 0; i < height && (i + y) < surface->height; i++){
            uint32_t yOffset = (i + y) * (surface->width);
            
            if(_width > 0)
                memset32_optimized((void*)(buffer + (yOffset + x)), colour_i, _width);
        }
    }

    void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface){
        DrawRect(x,y,width,height,colour.r,colour.g,colour.b,surface);
    }

    uint32_t Interpolate(double q11, double q21, double q12, double q22, double x, double y){
        double val1 = q11;
        double val2 = q21;
        double x1 = (val1 + (val2 - val1) * (x - ((int)x)));

        val1 = q12;
        val2 = q22;
        double x2 = (val1 + (val2 - val1) * (x - ((int)x)));

        double val = (x1 + (x2 - x1) * (y - ((int)y)));
        return (uint32_t)val;
    }

    void DrawGradient(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface){
        if(x < 0){
            width += x;
            x = 0;
        }

        if(y < 0){
            height += y;
            y = 0;
        }

        for(int j = 0; j < width && (x + j) < surface->width; j++){
                DrawRect(x + j, y, 1, height, (uint8_t)(j*(((double)c2.r - (double)c1.r)/width)+c1.r),(uint8_t)(j*(((double)c2.g - (double)c1.g)/width)+c1.g),(uint8_t)(j*(((double)c2.b - (double)c1.b)/width)+c1.b),surface);
        }
    }

    void DrawGradientVertical(rect_t rect, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface){
        DrawGradientVertical(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, c1, c2, surface);
    }

    void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface){
        if(x < 0){
            width += x;
            x = 0;
        }

        if(y < 0){
            height += y;
            y = 0;
        }
        
        width = (width + x > surface->width) ? (surface->width - x) : width;

        for(int j = 0; j < height && (y + j) < surface->height; j++){
                DrawRect(x, y + j, width, 1, (uint8_t)(j*(((double)c2.r - (double)c1.r)/height)+c1.r),(uint8_t)(j*(((double)c2.g - (double)c1.g)/height)+c1.g),(uint8_t)(j*(((double)c2.b - (double)c1.b)/height)+c1.b),surface);
        }
    }

    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset){
        for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
            int rowSize = ((offset.x + src->width) > dest->width) ? dest->width - offset.x : src->width;

            if(rowSize <= 0) return;

            memcpy_optimized(dest->buffer + ((i+offset.y)*(dest->width*4) + offset.x*4), src->buffer + i*src->width*4, rowSize*4);
        }
    }

    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion){
        int srcWidth = (srcRegion.pos.x + srcRegion.size.x) > src->width ? (src->width - srcRegion.pos.x) : srcRegion.size.x;
        int srcHeight = (srcRegion.pos.y + srcRegion.size.y) > src->height ? (src->height - srcRegion.pos.y) : srcRegion.size.y;
        int rowSize = ((offset.x + srcWidth) > dest->width) ? dest->width - offset.x : srcWidth;

        if(rowSize <= 0) return;

        for(int i = 0; i < srcHeight && i < dest->height - offset.y; i++){
            memcpy_optimized(dest->buffer + ((i+offset.y)*(dest->width*4) + offset.x*4), src->buffer + (i + srcRegion.pos.y)*src->width * 4 + srcRegion.pos.x * 4, rowSize*4);
        }
    }

    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset){
        uint32_t* srcBuffer = (uint32_t*)src->buffer;
        uint32_t* destBuffer = (uint32_t*)dest->buffer;
        for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
            for(int j = 0; j < src->width && j < dest->width - offset.x; j++){
                if((srcBuffer[i*src->width + j] >> 24) < 255) continue;
                destBuffer[(i+offset.y)*dest->width + j + offset.x] = srcBuffer[i*src->width + j];
            }
        }
    }
    
    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion){
        int srcWidth = (srcRegion.pos.x + srcRegion.size.x) > src->width ? (src->width - srcRegion.pos.x) : srcRegion.size.x;
        int srcHeight = (srcRegion.pos.y + srcRegion.size.y) > src->height ? (src->height - srcRegion.pos.y) : srcRegion.size.y;
        int rowSize = ((offset.x + srcWidth) > dest->width) ? dest->width - offset.x : srcWidth;

        if(rowSize <= 0) return;

        uint32_t* srcBuffer = (uint32_t*)src->buffer;
        uint32_t* destBuffer = (uint32_t*)dest->buffer;

        for(int i = 0; i < srcHeight && i < dest->height - offset.y; i++){
            for(int j = 0; j < srcWidth && j < dest->width - offset.x; j++){
                if((srcBuffer[i*src->width + j] >> 24) < 255) continue;
                destBuffer[((i+offset.y)*(dest->width) + offset.x) + j] = srcBuffer[(i + srcRegion.pos.y)*src->width + srcRegion.pos.x + j];
            }
        }
    }
}