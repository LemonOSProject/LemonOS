#include <gfx/graphics.h>

#include <core/syscall.h>

void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface){
    uint32_t colour_i = (colour.a << 24) | (colour.r << 16) | (colour.g << 8) | colour.b;
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int i = 0; i < rect.size.y; i++){
        for(int j = 0; j < rect.size.x; j++){
            buffer[(i + rect.pos.y) * surface->width + (j + rect.pos.x)] = colour_i;
        }
    }
}

video_mode_t GetVideoMode(){
    video_mode_t v;
    syscall(SYS_GETVIDEOMODE, (uint32_t)&v, 0, 0, 0, 0);
    return v;
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
    
    uint32_t colour_i = (r << 16) | (g << 8) | b;
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int i = 0; i < height && (i + y) < surface->height; i++){
        for(int j = 0; j < width && (x + j) < surface->width; j++){
            buffer[(i + y) * surface->width + (j + x)] = colour_i;
        }
    }
}

void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface){
    if(x < 0){
        width += x;
        x = 0;
    }

    if(y < 0){
        height += y;
        y = 0;
    }
    
    uint32_t colour_i = (colour.r << 16) | (colour.g << 8) | colour.b;
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int i = 0; i < height && (i + y) < surface->height; i++){
        for(int j = 0; j < width && (x + j) < surface->width; j++){
            buffer[(i + y) * surface->width + (j + x)] = colour_i;
        }
    }
}

int floor(double num) {
	int x = (int)num;
	return num < x ? x - 1 : x;
}


void DrawBitmapImage(int x, int y, int w, int h, uint8_t *data, surface_t* surface) {
    uint32_t rowSize = floor((24*w + 31) / 32) * 4;
    uint32_t bmpOffset = 0;

    uint32_t pixelSize = 4;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint32_t offset = (y + i)*surface->width*4 + (x + j) * pixelSize;
            surface->buffer[offset] = data[bmpOffset + j * (24 / 8)];
            surface->buffer[offset + 1] = data[bmpOffset + j * (24 / 8) + 1];
            surface->buffer[offset + 2] = data[bmpOffset + j * (24 / 8) + 2];
        }
        bmpOffset += rowSize;
    }
}