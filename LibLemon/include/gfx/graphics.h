#pragma once

#include <stdint.h>
#include <gfx/surface.h>

typedef struct Vector2i{
    int x, y;
} vector2i_t; // Two dimensional integer vector

typedef struct Rect{
    vector2i_t pos;
    vector2i_t size;
} rect_t; // Rectangle

typedef struct RGBAColour{
    uint8_t r, g, b, a; /* Red, Green, Blue, Alpha (Transparency) Respectively*/
} rgba_colour_t;

typedef struct{
    uint32_t width; // Resolution width
    uint32_t height; // Resolution height
    uint16_t bpp; // Resolution depth/bits per pixel

    uint32_t pitch; // Video mode pitch

    uint32_t address; // Video memory address
} video_mode_t;

// DrawRect (rect, colour, surface*) - Draw filled rectangle
void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface);
void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface);

void DrawChar(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
void DrawString(char* str, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
void DrawBitmapImage(int x, int y, int w, int h, uint8_t *data, surface_t* surface);

video_mode_t GetVideoMode();
