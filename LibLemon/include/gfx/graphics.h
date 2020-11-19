#pragma once

#include <stdint.h>
#include <gfx/surface.h>
#include <gfx/types.h>
#include <png.h>
#include <string.h>
#include <gfx/font.h>
#include <gfx/text.h>
#include <list>

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

namespace Lemon::Graphics{
    enum {
        SizeUnitPixels,
        SizeUnitPercentage,        
    };
    
    enum {
        PositionAbsolute,
        PositionRelative,        
    };

    enum ImageType {
        Image_Unknown,
        Image_BMP,
        Image_PNG,
    };

    // Check for BMP signature
    static inline bool IsBMP(const void* data){
        return (strncmp(((bitmap_file_header_t*)data)->magic,"BM", 2) == 0);
    }

    // Check for PNG signature
    bool IsPNG(const void* data);

    static inline int IdentifyImage(const void* data){
        int type = 0;

        if(IsBMP(data)){
            type = Image_BMP;
        } else if (IsPNG(data)){
            type = Image_PNG;
        } else type = Image_Unknown;

        return type;
    }

    static inline uint32_t AlphaBlend(uint32_t oldColour, uint8_t r, uint8_t g, uint8_t b, double opacity){
        int oldB = oldColour & 0xFF;
        int oldG = (oldColour >> 8) & 0xFF;
        int oldR = (oldColour >> 16) & 0xFF;
        uint32_t newColour = (int)(b * opacity + oldB * (1 - opacity)) | (((int)(g * opacity + oldG * (1 - opacity)) << 8)) | (((int)(r * opacity + oldR * (1 - opacity)) << 16));

        return newColour;
    }

    // PointInRect (rect, point) - Check if a point lies inside a rectangle
    bool PointInRect(rect_t rect, vector2i_t point);

    rgba_colour_t AverageColour(rgba_colour_t c1, rgba_colour_t c2);

    // DrawRect (rect, colour, surface*) - Draw filled rectangle
    void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface, rect_t mask = {0, 0, INT_MAX, INT_MAX});
    void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t mask = {0, 0, INT_MAX, INT_MAX});
    void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface, rect_t mask = {0, 0, INT_MAX, INT_MAX});

    void DrawRectOutline(rect_t rect, rgba_colour_t colour, surface_t* surface);
    void DrawRectOutline(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
    void DrawRectOutline(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface);

    int DrawChar(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits, Font* font = DefaultFont());
    int DrawChar(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font = DefaultFont());
    int DrawChar(char c, int x, int y, rgba_colour_t col, surface_t* surface, Font* font = DefaultFont());
    int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits, Font* font = DefaultFont());
    int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font = DefaultFont());
    int DrawString(const char* str, int x, int y, rgba_colour_t colour, surface_t* surface, rect_t limits, Font* font = DefaultFont());
    int DrawString(const char* str, int x, int y, rgba_colour_t colour, surface_t* surface, Font* font = DefaultFont());
    int GetTextLength(const char* str, Font* font);
    int GetTextLength(const char* str, size_t n, Font* font);
    int GetTextLength(const char* str);
    int GetTextLength(const char* str, size_t n);
    int GetCharWidth(char c, Font* font);
    int GetCharWidth(char c);

    uint32_t Interpolate(double q11, double q21, double q12, double q22, double x, double y);

    // LoadImage (const char*, int, int, int, int, surface_t*, bool) - Load image, scale to dimensions (w, h) and copy to surface at offset (x, y)
    int LoadImage(const char* path, int x, int y, int w, int h, surface_t* surface, bool preserveAspectRatio);
    // LoadImage (FILE* f, surface_t* surface) - Load image from open file and create a new surface
    int LoadImage(FILE* f, surface_t* surface);
    // LoadImage (const char* path, surface_t* surface) - Attempt to load image at path and create a new surface
    int LoadImage(const char* path, surface_t* surface);
    int LoadPNGImage(FILE* f, surface_t* surface);
    int SavePNGImage(FILE* f, surface_t* surface, bool writeTransparency);
    int LoadBitmapImage(FILE* f, surface_t* surface);
    int DrawImage(int x, int y, int w, int h, uint8_t *data, size_t dataSz, surface_t* surface, bool preserveAspectRatio);
    int DrawBitmapImage(int x, int y, int w, int h, uint8_t *data, surface_t* surface, bool preserveAspectRatio = false);

    void DrawGradient(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
    void DrawGradientVertical(rect_t rect, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
    void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
    void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface, rect_t limits);

    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset = {0,0});
    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion);
    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset = {0,0});
    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion);
}
