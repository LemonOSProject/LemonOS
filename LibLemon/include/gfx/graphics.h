#pragma once

#include <stdint.h>
#include <gfx/surface.h>
#include <png.h>
#include <string.h>
#include <gfx/font.h>
#include <list>

typedef struct Vector2i{
    int x, y;
} vector2i_t; // Two dimensional integer vector

inline vector2i_t operator+ (const vector2i_t& l, const vector2i_t& r){
    return {l.x + r.x, l.y + r.y};
}

inline void operator+= (vector2i_t& l, const vector2i_t& r){
    l = l + r;
}

inline vector2i_t operator- (const vector2i_t& l, const vector2i_t& r){
    return {l.x - r.x, l.y - r.y};
}

inline void operator-= (vector2i_t& l, const vector2i_t& r){
    l = l - r;
}

typedef struct Rect{
    union {
        vector2i_t pos;
        struct {
            int x;
            int y;
        };
    };
    union {
        vector2i_t size;
        struct {
            int width;
            int height;
        };
    };

    int left() { return x; }
    int right() { return x + width; }
    int top() { return y; }
    int bottom() { return y + height; }

    int left(int newLeft) { width += x - newLeft; x = newLeft; return x; }
    int right(int newRight) { width += newRight - (x + width); return x + width; }
    int top(int newTop) { height += y - newTop; y = newTop; return y; }
    int bottom(int newBottom) { height += newBottom - (y + height); return y + height; }

    std::list<Rect> Split(Rect cut){
        std::list<Rect> clips;
        Rect victim = *this;

        if(cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(cut.left()); // Left of cutting rect's left edge

            victim.left(cut.left());

            clips.push_back(clip);
        }

        if(cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(cut.top()); // Above cutting rect's top edge
            clip.right(victim.right());

            victim.top(cut.top());

            clips.push_back(clip);
        }

        if(cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
            Rect clip;
            clip.top(victim.top());
            clip.left(cut.right());
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.right(cut.right());

            clips.push_back(clip);
        }

        if(cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
            Rect clip;
            clip.top(cut.bottom());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.bottom(cut.bottom());

            clips.push_back(clip);
        }

        return clips;
    }
} rect_t; // Rectangle

typedef struct RGBAColour{
    uint8_t r, g, b, a; /* Red, Green, Blue, Alpha (Transparency) Respectively*/
} __attribute__ ((packed)) rgba_colour_t;

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

    // PointInRect (rect, point) - Check if a point lies inside a rectangle
    bool PointInRect(rect_t rect, vector2i_t point);

    // DrawRect (rect, colour, surface*) - Draw filled rectangle
    void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface);
    void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
    void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface);

    void DrawRectOutline(rect_t rect, rgba_colour_t colour, surface_t* surface);
    void DrawRectOutline(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
    void DrawRectOutline(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface);

    Font* DefaultFont();
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
    int LoadBitmapImage(FILE* f, surface_t* surface);
    int DrawImage(int x, int y, int w, int h, uint8_t *data, size_t dataSz, surface_t* surface, bool preserveAspectRatio);
    int DrawBitmapImage(int x, int y, int w, int h, uint8_t *data, surface_t* surface, bool preserveAspectRatio = false);
    
    void InitializeFonts();
    void RefreshFonts();
    Font* LoadFont(const char* path, const char* id = nullptr, int sz = 12);
    Font* GetFont(const char* id);

    void DrawGradient(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
    void DrawGradientVertical(rect_t rect, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
    void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);

    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset = {0,0});
    void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion);
    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset = {0,0});
    void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset, rect_t srcRegion);
}
