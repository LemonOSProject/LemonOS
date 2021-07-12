#pragma once

#include <Lemon/Graphics/Font.h>
#include <Lemon/Graphics/Surface.h>
#include <Lemon/Graphics/Text.h>
#include <Lemon/Graphics/Types.h>

#include <png.h>
#include <stdint.h>
#include <string.h>

#include <list>

typedef struct {
    char magic[2];     // Magic number = should be equivalent to "BM"
    uint32_t size;     // Size of file
    uint32_t reserved; // Reserved bytes
    uint32_t offset;   // Offset of pixel data
} __attribute__((packed)) bitmap_file_header_t;

typedef struct {
    uint32_t hdrSize;          // The size of this header
    int32_t width;             // The width of the image in pixels
    int32_t height;            // The height of the image in pixels
    uint16_t colourPlanes;     // Number of colour planes (should be 1)
    uint16_t bpp;              // Bits per pixel
    uint32_t compression;      // Compression method
    uint32_t size;             // The image size
    int32_t hres;              // The horizontal resolution of the image (pixels per metre)
    int32_t vres;              // The vertical resolution of the image (pixels per metre)
    uint32_t colourNum;        // The number of colours in the colour palette
    uint32_t importantColours; // Number of important colours - usually ignored
} __attribute__((packed)) bitmap_info_header_t;

typedef struct {
    uint32_t hdrSize;
    int32_t width;
    int32_t height;
    uint16_t colourPlanes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t size;
    int32_t hres;
    int32_t vres;
    uint32_t clrUsed;
    uint32_t clrImportant;
    uint32_t redMask;
    uint32_t greenMask;
    uint32_t blueMask;
    uint32_t alphaMask;
} __attribute__((packed)) bitmap_info_headerv4_t;

namespace Lemon::Graphics {
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

class Texture final {
  public:
    enum TextureScaling {
        ScaleNone,
        ScaleFit,
        ScaleFill,
    };

    Texture(vector2i_t size);
    ~Texture();

    /////////////////////////////
    /// \brief Set Texture size
    ///
    /// Set the size of the texture and updates surface
    ///
    /// \param newSize New size of texture
    /////////////////////////////
    void SetSize(vector2i_t newSize);

    /////////////////////////////
    /// \brief Set whether or not alpha channel is used
    /////////////////////////////
    inline void SetAlpha(bool v) { alpha = v; } // Set whether the alpha channel is enabled

    /////////////////////////////
    /// \brief Set texture scaling mode
    /////////////////////////////
    inline void SetScaling(TextureScaling s) {
        scaling = s;
        UpdateSurface();
    } // Set whether the alpha channel is enabled

    /////////////////////////////
    /// \brief Render TextObject on surface
    ///
    /// \param surface Surface to render to
    /////////////////////////////
    void Blit(vector2i_t pos, surface_t* surface);

    /////////////////////////////
    /// \brief Loads new texture source data
    ///
    /// Loads new source data and updates the render surface
    ///
    /// \param sourcePixels Source pixel data
    /////////////////////////////
    void LoadSourcePixels(surface_t* sourcePixels);

    /////////////////////////////
    /// \brief Loads new texture source data, assuming ownership of the buffer
    ///
    /// Loads new source data assuming ownership of the buffer and updates the render surface
    ///
    /// \param sourcePixels Source pixel data
    /////////////////////////////
    void AdoptSourcePixels(surface_t* sourcePixels);

    void UpdateSurface();

    inline Vector2i Size() const { return size; }

  protected:
    TextureScaling scaling = ScaleFit;

    surface_t source = {.width = 0, .height = 0, .depth = 32, .buffer = nullptr}; // Source pixels
    surface_t surface;                                                            // Result pixels

    Vector2i size;
    bool alpha = false;
};

// Check for BMP signature
static inline bool IsBMP(const void* data) { return (strncmp(((bitmap_file_header_t*)data)->magic, "BM", 2) == 0); }

// Check for PNG signature
bool IsPNG(const void* data);

static inline int IdentifyImage(const void* data) {
    int type = 0;

    if (IsBMP(data)) {
        type = Image_BMP;
    } else if (IsPNG(data)) {
        type = Image_PNG;
    } else
        type = Image_Unknown;

    return type;
}

static inline uint32_t AlphaBlend(uint32_t oldColour, uint8_t r, uint8_t g, uint8_t b, double opacity) {
    int oldB = oldColour & 0xFF;
    int oldG = (oldColour >> 8) & 0xFF;
    int oldR = (oldColour >> 16) & 0xFF;
    uint32_t newColour = (int)(b * opacity + oldB * (1 - opacity)) |
                         (((int)(g * opacity + oldG * (1 - opacity)) << 8)) |
                         (((int)(r * opacity + oldR * (1 - opacity)) << 16));

    return newColour;
}

// PointInRect (rect, point) - Check if a point lies inside a rectangle
bool PointInRect(rect_t rect, vector2i_t point);

rgba_colour_t AverageColour(rgba_colour_t c1, rgba_colour_t c2);

// DrawRect (rect, colour, surface*) - Draw filled rectangle
void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface,
              rect_t mask = {0, 0, INT_MAX, INT_MAX});
inline void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface, rect_t mask = {0, 0, INT_MAX, INT_MAX}) {
    DrawRect(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, colour.r, colour.g, colour.b, surface, mask);
}
inline void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface,
                     rect_t mask = {0, 0, INT_MAX, INT_MAX}) {
    DrawRect(x, y, width, height, colour.r, colour.g, colour.b, surface, mask);
}

void DrawRectOutline(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface,
                     rect_t mask);
void DrawRectOutline(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface);
inline void DrawRectOutline(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface) {
    DrawRectOutline(x, y, width, height, colour.r, colour.g, colour.b, surface);
}
inline void DrawRectOutline(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface,
                            rect_t mask) {
    DrawRectOutline(x, y, width, height, colour.r, colour.g, colour.b, surface, mask);
}
inline void DrawRectOutline(rect_t rect, rgba_colour_t colour, surface_t* surface) {
    DrawRectOutline(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, colour, surface);
}
inline void DrawRectOutline(rect_t rect, rgba_colour_t colour, surface_t* surface, rect_t mask) {
    DrawRectOutline(rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, colour, surface, mask);
}

int DrawChar(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits,
             Font* font = DefaultFont());
int DrawChar(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font = DefaultFont());
int DrawChar(char c, int x, int y, rgba_colour_t col, surface_t* surface, Font* font = DefaultFont());
int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits,
               Font* font = DefaultFont());
int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface,
               Font* font = DefaultFont());
int DrawString(const char* str, int x, int y, rgba_colour_t colour, surface_t* surface, rect_t limits,
               Font* font = DefaultFont());
int DrawString(const char* str, int x, int y, rgba_colour_t colour, surface_t* surface, Font* font = DefaultFont());
int GetTextLength(const char* str, Font* font);
int GetTextLength(const char* str, size_t n, Font* font);
int GetTextLength(const char* str);
int GetTextLength(const char* str, size_t n);
int GetCharWidth(char c, Font* font);
int GetCharWidth(char c);

uint32_t Interpolate(double q11, double q21, double q12, double q22, double x, double y);

// LoadImage (const char*, int, int, int, int, surface_t*, bool) - Load image, scale to dimensions (w, h) and copy to
// surface at offset (x, y)
int LoadImage(const char* path, int x, int y, int w, int h, surface_t* surface, bool preserveAspectRatio);
// LoadImage (FILE* f, surface_t* surface) - Load image from open file and create a new surface
int LoadImage(FILE* f, surface_t* surface);
// LoadImage (const char* path, surface_t* surface) - Attempt to load image at path and create a new surface
int LoadImage(const char* path, surface_t* surface);
int LoadPNGImage(FILE* f, surface_t* surface);
int SavePNGImage(FILE* f, surface_t* surface, bool writeTransparency);
int LoadBitmapImage(FILE* f, surface_t* surface);
int DrawImage(int x, int y, int w, int h, uint8_t* data, size_t dataSz, surface_t* surface, bool preserveAspectRatio);
int DrawBitmapImage(int x, int y, int w, int h, uint8_t* data, surface_t* surface, bool preserveAspectRatio = false);

void DrawGradient(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
void DrawGradientVertical(rect_t rect, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
void DrawGradientVertical(rect_t rect, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface, rect_t limits);
void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface);
void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface,
                          rect_t limits);

void surfacecpy(surface_t* dest, const surface_t* src, vector2i_t offset = {0, 0});
void surfacecpy(surface_t* dest, const surface_t* src, vector2i_t offset, rect_t srcRegion);
void surfacecpyTransparent(surface_t* dest, const surface_t* src, vector2i_t offset = {0, 0});
void surfacecpyTransparent(surface_t* dest, const surface_t* src, vector2i_t offset, rect_t srcRegion);

void Blit(Surface* dest, const Surface* src);
void Blit(Surface* dest, const Surface* src, vector2i_t offset);
void Blit(Surface* dest, const Surface* src, vector2i_t offset, rect_t region);

void BlitRounded(Surface* dest, const Surface* src, vector2i_t offset, int radius);
} // namespace Lemon::Graphics
