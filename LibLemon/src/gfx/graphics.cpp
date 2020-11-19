#include <gfx/graphics.h>

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <assert.h>

extern "C" void memcpy_sse2(void* dest, void* src, size_t count);
extern "C" void memcpy_sse2_unaligned(void* dest, void* src, size_t count);
extern "C" void memset32_sse2(void* dest, uint32_t c, uint64_t count);
extern "C" void memset64_sse2(void* dest, uint64_t c, uint64_t count);

void memset32_optimized(void* _dest, uint32_t c, size_t count){
    uint32_t* dest = reinterpret_cast<uint32_t*>(_dest);
    while(count--){
        *(dest++) = c;
    }
    return;
}

void memset64_optimized(void* _dest, uint64_t c, size_t count) {
    uint64_t* dest = reinterpret_cast<uint64_t*>(_dest);
    if(((size_t)dest & 0x7)){
        while(count--){
            *(dest++) = c;
        }
        return;
    }

    size_t overflow = (count & 0x7); // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    memset64_sse2(dest, c, size_aligned >> 3);

    while(overflow--){
        *(dest++) = c;
    }
}

extern "C"
void memcpy_optimized(void* dest, void* src, size_t count);

namespace Lemon::Graphics{

    // Check if a point lies inside a rectangle
    bool PointInRect(rect_t rect, vector2i_t point){
        return (point.x >= rect.pos.x && point.x < rect.pos.x + rect.size.x && point.y >= rect.pos.y && point.y < rect.pos.y + rect.size.y);
    }

    rgba_colour_t AverageColour(rgba_colour_t c1, rgba_colour_t c2){
        int r = static_cast<int>(c1.r) + c2.r;
        int g = static_cast<int>(c1.g) + c2.g;
        int b = static_cast<int>(c1.b) + c2.b;
        int a = static_cast<int>(c1.a) + c2.a;

        return {static_cast<uint8_t>(r / 2), static_cast<uint8_t>(g / 2), static_cast<uint8_t>(b / 2), static_cast<uint8_t>(a / 2)};
    }

    void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface, rect_t mask){
        DrawRect(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, colour, surface, mask);
    }

    void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t mask){
        if(x < 0){
            width += x;
            x = 0;
        }

        if(y < 0){
            height += y;
            y = 0;
        }

        if(mask.x > x){
            int xChange = mask.x - x;

            width -= xChange;
            
            x = mask.x;
        }

        if(mask.y > y){
            int yChange = mask.y - y;

            height -= yChange;

            y = mask.y;
        }

        if(x + width > mask.x + mask.width){
            width = mask.width - x;
        }

        if(y + height > mask.y + mask.height){
            height = mask.height - y;
        }

        if(width <= 0 || height <= 0){
            return;
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

    void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface, rect_t mask){
        DrawRect(x, y, width, height, colour.r, colour.g, colour.b, surface, mask);
    }

    void DrawRectOutline(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface){
        DrawRect(x, y, width, 1, r, g, b, surface);
        DrawRect(x, y + 1, 1, height - 1, r, g, b, surface);
        DrawRect(x, y + height - 1, width, 1, r, g, b, surface);
        DrawRect(x + width - 1, y + 1, 1, height - 1, r, g, b, surface);
    }
    
    void DrawRectOutline(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface){
        DrawRectOutline(x,y,width,height,colour.r,colour.g,colour.b,surface);
    }
    
    void DrawRectOutline(rect_t rect, rgba_colour_t colour, surface_t* surface){
        DrawRectOutline(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, colour, surface);
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

    void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface, rect_t limits){
        if(x < 0){
            width += x;
            x = 0;
        }

        if(y < 0){
            height += y;
            y = 0;
        }
        
        width = (width + x > surface->width) ? (surface->width - x) : width;

        int j = 0;
        if(limits.pos.y > y){
            j = limits.pos.y - y; // Its important that we change j instead of y for the gradient calculation
        }

        if(limits.pos.x > x){
            width -= (limits.pos.x - x);
            x = limits.pos.x;
        }

        if(x + width > limits.pos.x + limits.size.x){
            width = limits.pos.x - x + limits.size.x;
        }

        for(; j < height && (y + j) < surface->height && (y + j) < limits.pos.y + limits.size.y; j++){
            DrawRect(x, y + j, width, 1, (uint8_t)(j*(((double)c2.r - (double)c1.r)/height)+c1.r),(uint8_t)(j*(((double)c2.g - (double)c1.g)/height)+c1.g),(uint8_t)(j*(((double)c2.b - (double)c1.b)/height)+c1.b),surface);
        }
    }

    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset){
        if(dest->height == src->height && dest->width == src->width && offset.x == 0 && offset.y == 0) {
            memcpy_optimized(dest->buffer, src->buffer, dest->width * dest->height);
            return;
        }

        int rowSize = ((offset.x + src->width) > dest->width) ? dest->width - offset.x : src->width;
        int rowOffset = 0;

        if(offset.x < 0){
            rowOffset = -offset.x * 4;
            rowSize += offset.x;
            offset.x = 0;
        }

        int i = 0;

        if(offset.y < 0){
            i += offset.y;
            offset.y = 0;
        }

        for(; i < src->height && i < dest->height - offset.y; i++){
            if(rowSize <= 0) return;

            memcpy_optimized(dest->buffer + ((i+offset.y)*(dest->width*4) + offset.x*4), src->buffer + i*src->width*4 + rowOffset, rowSize);
        }
    }

    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion){
        if(offset.x >= dest->width || offset.y >= dest->height || srcRegion.pos.x >= src->width || srcRegion.pos.y >= src->height) return;

        int srcWidth = (srcRegion.pos.x + srcRegion.size.x) > src->width ? (src->width - srcRegion.pos.x) : srcRegion.size.x;
        int srcHeight = (srcRegion.pos.y + srcRegion.size.y) > src->height ? (src->height - srcRegion.pos.y) : srcRegion.size.y;
        int rowOffset = srcRegion.pos.x;
        int rowSize = srcWidth;

        if(offset.x < 0){
            rowOffset += abs(offset.x);
            rowSize += offset.x;
            offset.x = 0;
        }

        if(rowOffset + rowSize >= src->width){
            rowSize = src->width - rowOffset;
        }

        if(offset.x + rowSize >= dest->width){
            rowSize = dest->width - offset.x;
        }

        int i = 0;

        if(offset.y < 0){
            i += abs(offset.y);
            offset.y = 0;
        }

        if(rowSize <= 0 || rowOffset >= src->width) return;
        
        unsigned destPitch = dest->width << 2;
        unsigned srcPitch = src->width << 2;

        for(; i < srcHeight && (i + offset.y) < dest->height; i++){
            memcpy_optimized(dest->buffer + ((i + offset.y) * destPitch + (offset.x << 2)), src->buffer + (i + srcRegion.pos.y) * srcPitch + (rowOffset << 2), rowSize);
        }
    }

    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset){
        uint32_t* srcBuffer = (uint32_t*)src->buffer;
        uint32_t* destBuffer = (uint32_t*)dest->buffer;
        for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
            for(int j = 0; j < src->width && j < dest->width - offset.x; j++){
                uint32_t sPixel = (srcBuffer[i*src->width + j]);
                if(!((sPixel >> 24) & 0xFF)) continue; // Check for 0 alpha

                if(((sPixel >> 24) & 0xFF) >= 255){ // Check for full alpha
                    destBuffer[(i+offset.y)*dest->width + j + offset.x] = srcBuffer[i*src->width + j];
                } else {
                    destBuffer[(i+offset.y)*dest->width + j + offset.x] = AlphaBlend(destBuffer[(i+offset.y)*dest->width + j + offset.x], (sPixel >> 16) & 0xFF, (sPixel >> 8) & 0xFF, sPixel & 0xFF, ((sPixel >> 24) & 0xFF) * 1.0 / 255);
                }
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
                uint32_t sPixel = (srcBuffer[i*src->width + j]);
                if(!((sPixel >> 24) & 0xFF)) continue; // Check for 0 alpha

                if(((sPixel >> 24) & 0xFF) >= 255){ // Check for full alpha
                    destBuffer[((i+offset.y)*(dest->width) + offset.x) + j] = srcBuffer[(i + srcRegion.pos.y)*src->width + srcRegion.pos.x + j];
                } else {
                    destBuffer[((i+offset.y)*(dest->width) + offset.x) + j] = AlphaBlend(destBuffer[((i+offset.y)*(dest->width) + offset.x) + j], (sPixel >> 16) & 0xFF, (sPixel >> 8) & 0xFF, sPixel & 0xFF, ((sPixel >> 24) & 0xFF) * 1.0 / 255);
                }
            }
        }
    }
}
