#include <Lemon/Graphics/Font.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Text.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <ctype.h>

extern uint8_t font_default[];

namespace Lemon::Graphics {
extern int fontState;
extern Font* mainFont;

int DrawChar(char character, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits,
             Font* font) {
    if (!isprint(character)) {
        return 0;
    }

    if (y >= surface->height || x >= surface->width || y >= limits.height || x >= limits.width)
        return 0;

    if (fontState == -1) { //
        character &= 0x7F;

        for (int i = 0; i < 12; i++) {
            uint8_t line = font_default[i * 128 + character];

            for (int j = 0; j < 8; j++) {
                if (line & 0x80)
                    DrawRect(j + x, i + y, 1, 1, r, g, b, surface);
                line <<= 1; // Shift line left by one
            }
        }
        return 8;
    } else if (fontState != 1 || !font->face)
        InitializeFonts();

    uint32_t colour_i = 0xFF000000 | (r << 16) | (g << 8) | b;
    uint32_t* buffer = (uint32_t*)surface->buffer;

    FT_Face face = (FT_Face)font->face;
    if (int err = FT_Load_Char(face, character, FT_LOAD_RENDER)) {
        return 0;
    }

    int maxHeight = font->lineHeight - (font->height - face->glyph->bitmap_top);

    if (y + maxHeight >= limits.y + limits.height) {
        maxHeight = limits.y + limits.height - y;
    }

    if (y + maxHeight >= surface->height) {
        maxHeight = surface->height - y;
    }

    if (y < 0 && -y > maxHeight) {
        return 0;
    }

    int i = 0;
    if (y < 0) {
        i = -y;
    }

    if (y + (font->height - face->glyph->bitmap_top) < limits.y) {
        i = limits.y - (y + (font->height - face->glyph->bitmap_top));
    }

    for (; i < static_cast<int>(face->glyph->bitmap.rows) && i < maxHeight; i++) {
        if (y + i < 0)
            continue;

        uint32_t yOffset = (i + y + (font->height - face->glyph->bitmap_top)) * (surface->width);
        for (unsigned int j = 0; j < face->glyph->bitmap.width && static_cast<long>(j) + x < surface->width;
             j++) {
            if (x + static_cast<long>(j) < 0)
                continue;
            if (face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j] == 255)
                buffer[yOffset + (j + x)] = colour_i;
            else if (face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j] > 0) {
                buffer[yOffset + (j + x)] = AlphaBlendInt(buffer[yOffset + (j + x)], RGBAColour::ToARGB({r, g, b, face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j]}));
            }
        }
    }
    return face->glyph->advance.x >> 6;
}

int DrawChar(char character, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font) {
    return DrawChar(character, x, y, r, g, b, surface, {0, 0, surface->width, surface->height}, font);
}

int DrawChar(char character, int x, int y, rgba_colour_t col, surface_t* surface, Font* font) {
    return DrawChar(character, x, y, col.r, col.g, col.b, surface, font);
}

Font* DefaultFont() { return mainFont; }

int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits,
               Font* font) {
    if (y >= surface->height || x >= surface->width || y >= limits.y + limits.height || x >= limits.x + limits.width)
        return 0;

    if (fontState == -1) {
        int xOffset = 0;
        while (*str != 0) {
            DrawChar(*str, x + xOffset, y, r, g, b, surface);
            xOffset += 8;
            str++;
        }
        return xOffset;
    } else if (fontState != 1 || !font->face)
        InitializeFonts();

    uint32_t colour_i = 0xFF000000 | (r << 16) | (g << 8) | b;
    uint32_t* buffer = (uint32_t*)surface->buffer;

    unsigned int maxHeight = font->lineHeight;

    if (y < 0 && static_cast<unsigned>(-y) > maxHeight) {
        return 0;
    }

    if (y + static_cast<int>(maxHeight) >= limits.y + limits.height) {
        maxHeight = limits.y + limits.height - y;
    }

    if (y + static_cast<int>(maxHeight) >= surface->height) {
        maxHeight = surface->height - y;
    }

    FT_Face face = (FT_Face)font->face;

    unsigned int lastGlyph = 0;
    int xOffset = x;
    while (*str != 0) {
        if (*str == '\n') {
            break;
        } else if (!isprint(*str)) {
            str++;
            continue;
        }

        unsigned int glyph = FT_Get_Char_Index(face, *str);
        if (FT_HAS_KERNING(face) && lastGlyph) {
            FT_Vector delta;

            FT_Get_Kerning(face, lastGlyph, glyph, FT_KERNING_DEFAULT, &delta);
            xOffset += delta.x >> 6;
        }
        lastGlyph = glyph;

        if (int err = FT_Load_Glyph(face, glyph, FT_LOAD_RENDER)) {
            str++;
            continue;
        }

        if (xOffset + (face->glyph->advance.x >> 6) < limits.x) {
            xOffset += face->glyph->advance.x >> 6;
            str++;
            continue;
        }

        unsigned i = 0;
        if (y < 0) {
            i = -y;
        }

        if (y + (font->height - face->glyph->bitmap_top) < limits.y) {
            i = limits.y - (y + (font->height - face->glyph->bitmap_top));
        }

        for (; i < face->glyph->bitmap.rows && i + (font->height - face->glyph->bitmap_top) < maxHeight;
             i++) {
            uint32_t yOffset = (i + y + (font->height - face->glyph->bitmap_top)) * (surface->width);

            unsigned j = 0;
            if (xOffset < 0) {
                j = -x;
            }

            if (xOffset < limits.x) {
                j = limits.x - xOffset;
            }

            for (; j < face->glyph->bitmap.width && (xOffset + static_cast<long>(j)) < surface->width; j++) {
                unsigned off = yOffset + (j + xOffset);
                if (face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j] == 255)
                    buffer[off] = colour_i;
                else if (face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j]) {
                    buffer[off] = AlphaBlendInt(buffer[off], RGBAColour::ToARGB({r, g, b, face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j]}));
                }
            }
        }

        xOffset += face->glyph->advance.x >> 6;
        str++;
    }
    return xOffset - x;
}

int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font) {
    return DrawString(str, x, y, r, g, b, surface, {0, 0, surface->width, surface->height}, font);
}

int DrawString(const char* str, int x, int y, rgba_colour_t col, surface_t* surface, rect_t limits, Font* font) {
    return DrawString(str, x, y, col.r, col.g, col.b, surface, limits, font);
}

int DrawString(const char* str, int x, int y, rgba_colour_t col, surface_t* surface, Font* font) {
    return DrawString(str, x, y, col.r, col.g, col.b, surface, font);
}

int GetCharWidth(char c, Font* font) {
    if (fontState == -1) {
        return 8;
    } else if (fontState != 1 || !font->face)
        InitializeFonts();

    if (c == '\n') {
        return 0;
    } else if (c == ' ') {
        return font->width;
    } else if (c == '\t') {
        return font->tabWidth * font->width;
    } else if (!isprint(c)) {
        return 0;
    }

    FT_Face face = (FT_Face)font->face;
    if (int err = FT_Load_Char(face, c, FT_LOAD_ADVANCE_ONLY)) {
        return 0;
    }

    return face->glyph->advance.x >> 6;
}

int GetCharWidth(char c) { return GetCharWidth(c, mainFont); }

int GetTextLength(const char* str, size_t n, Font* font) {
    if (fontState != 1 && fontState != -1)
        InitializeFonts();
    if (fontState == -1) {
        return strlen(str) * 8;
    }

    FT_Face face = (FT_Face)font->face;

    size_t len = 0;
    size_t i = 0;
    while (*str && i++ < n) {
        if (*str == '\n') {
            break;
        } else if (*str == ' ') {
            len += font->width;
            str++;
            continue;
        } else if (*str == '\t') {
            len += font->tabWidth * font->width;
            str++;
            continue;
        } else if (!isprint(*str)) {
            str++;
            continue;
        }

        if (int err = FT_Load_Char(face, *str, FT_LOAD_ADVANCE_ONLY)) {
            str++;
            continue;
        }

        len += face->glyph->advance.x >> 6;
        str++;
    }

    return len;
}

int GetTextLength(const char* str, Font* font) { return GetTextLength(str, strlen(str), font); }

int GetTextLength(const char* str, size_t n) { return GetTextLength(str, n, mainFont); }

int GetTextLength(const char* str) { return GetTextLength(str, strlen(str)); }

TextObject::TextObject(vector2i_t pos, const std::string& text, Font* font) {
    m_pos = pos;
    m_text = text;
    m_font = font;

    Update();
}

TextObject::TextObject(vector2i_t pos, const char* text, Font* font) {
    m_pos = pos;
    m_text = text;
    m_font = font;

    Update();
}

TextObject::TextObject(vector2i_t pos, Font* font) {
    m_pos = pos;
    m_font = font;

    Update();
}

void TextObject::Update() {
    m_textSize.x = GetTextLength(m_text.c_str(), m_font);
    m_textSize.y = m_font->lineHeight;

    if(!m_font){
        m_font = DefaultFont();
    }

    if(m_textSize.x <= 0){
        return; // Nothing to render
    }

    // Check if we need to reallocate the surface
    if(m_surface.width < m_textSize.x || m_surface.height < m_textSize.y) {
        if(m_surface.buffer){
            delete[] m_surface.buffer;
        }

        m_surface.buffer = new uint8_t[m_textSize.x * m_textSize.y * 4];
        m_surface.width = m_textSize.x;
        m_surface.height = m_textSize.y;
    }

    switch (m_renderMode) {
    case RenderNormal:
    default:
        memset(m_surface.buffer, 0, m_surface.BufferSize());
        DrawString(m_text.c_str(), 0, 0, m_colour, &m_surface, m_font);
        break;
    }
}
} // namespace Lemon::Graphics
