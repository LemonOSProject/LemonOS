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

typedef struct {
	char magic[2]; // Magic number = should be equivalent to "BM"
	uint32_t size; // Size of file
	uint32_t reserved; // Reserved bytes
	uint32_t offset; // Offset of pixel data
} __attribute__((packed)) bitmap_file_header_t;

typedef struct {
	uint32_t hdrSize; // The size of this header
	int32_t width; // The width of the image in pixels
	int32_t height; // The height of the image in pixels
	uint16_t colourPlanes;// Number of colour planes (should be 1)
	uint16_t bpp; // Bits per pixel
	uint32_t compression; // Compression method
	uint32_t size; // The image size
	int32_t hres; // The horizontal resolution of the image (pixels per metre)
	int32_t vres; // The vertical resolution of the image (pixels per metre)
	uint32_t colourNum; // The number of colours in the colour palette 
	uint32_t importantColours; // Number of important colours - usually ignored
} __attribute__((packed)) bitmap_info_header_t;

typedef struct {
  uint32_t        hdrSize;
  int32_t         width;
  int32_t         height;
  uint16_t        colourPlanes;
  uint16_t        bpp;
  uint32_t        compression;
  uint32_t        size;
  int32_t         hres;
  int32_t         vres;
  uint32_t        clrUsed;
  uint32_t        clrImportant;
  uint32_t        redMask;
  uint32_t        greenMask;
  uint32_t        blueMask;
  uint32_t        alphaMask;
} __attribute__((packed)) bitmap_info_headerv4_t;

// DrawRect (rect, colour, surface*) - Draw filled rectangle
void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface);
void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface);

void DrawChar(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
void DrawString(char* str, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
void DrawBitmapImage(int x, int y, int w, int h, uint8_t *data, surface_t* surface);

video_mode_t GetVideoMode();

void DrawGradient(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);

void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset);