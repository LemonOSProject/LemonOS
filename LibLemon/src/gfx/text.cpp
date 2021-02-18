#include <lemon/gfx/graphics.h>
#include <lemon/gfx/font.h>
#include <lemon/gfx/text.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <ctype.h>

extern uint8_t font_default[];

namespace Lemon::Graphics{
    extern int fontState;
    extern Font* mainFont;

    int DrawChar(char character, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits, Font* font){
        if (!isprint(character)) {
            return 0;
        }

        if(y >= surface->height || x >= surface->width || y >= limits.height || x >= limits.width) return 0;
        
        if((fontState != 1 && fontState != -1) || !font->face) InitializeFonts();
        if(fontState == -1){ //
            character &= 0x7F;

            for(int i = 0; i < 12; i++) {
                uint8_t line = font_default[i * 128 + character];

                for(int j = 0; j < 8; j++) {
                    if(line & 0x80)
                        DrawRect(j + x, i + y, 1, 1, r, g, b, surface);
                    line <<= 1; // Shift line left by one
                }
            }
            return 8;
        }

        uint32_t colour_i = 0xFF000000 | (r << 16) | (g << 8) | b;
        uint32_t* buffer = (uint32_t*)surface->buffer; 
        if(int err = FT_Load_Char(font->face, character, FT_LOAD_RENDER)) {
            printf("Freetype Error (code: %d, font: %s)\n", err, font->id);
            throw new FontException(FontException::FontRenderError, err);
            fontState = 0;
            return 0;
        }

        int maxHeight = font->lineHeight - (font->height - font->face->glyph->bitmap_top);

        if(y + maxHeight >= limits.y + limits.height){
            maxHeight = limits.y + limits.height - y;
        }

        if(y + maxHeight >= surface->height){
            maxHeight = surface->height - y;
        }

        if(y < 0 && -y > maxHeight){
            return 0;
        }

        int i = 0;
        if(y < 0){
            i = -y;
        }

        if(y + (font->height - font->face->glyph->bitmap_top) < limits.y){
            i = limits.y - (y + (font->height - font->face->glyph->bitmap_top));
        }
        
        for(; i < static_cast<int>(font->face->glyph->bitmap.rows) && i < maxHeight; i++){
            if(y + i < 0) continue;

            uint32_t yOffset = (i + y + (font->height - font->face->glyph->bitmap_top)) * (surface->width);
            for(unsigned int j = 0; j < font->face->glyph->bitmap.width && static_cast<long>(j) + x < surface->width; j++){
                if(x + static_cast<long>(j) < 0) continue;
                if(font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] == 255)
                    buffer[yOffset + (j + x)] = colour_i;
                else if(font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] > 0){
                    double val = font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] * 1.0 / 255;
                    buffer[yOffset + (j + x)] = AlphaBlend(buffer[yOffset + (j + x)], r, g, b, val);
                }
            }
        }
        return font->face->glyph->advance.x >> 6;
    }

    int DrawChar(char character, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font){
        return DrawChar(character, x, y, r, g, b, surface, {0, 0, surface->width, surface->height}, font);
    }

    int DrawChar(char character, int x, int y, rgba_colour_t col, surface_t* surface, Font* font){
        return DrawChar(character, x, y, col.r, col.g, col.b, surface, font);
    }
    
    Font* DefaultFont(){
        return mainFont;
    }

    int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, rect_t limits, Font* font) {
        if(y >= surface->height || x >= surface->width || y >= limits.y + limits.height || x >= limits.x + limits.width) return 0;

        if((fontState != 1 && fontState != -1) || !font->face) InitializeFonts();
        if(fontState == -1){
            int xOffset = 0;
            while (*str != 0) {
                DrawChar(*str, x + xOffset, y, r, g, b, surface);
                xOffset += 8;
                str++;
            }
            return xOffset;
        }

        uint32_t colour_i = 0xFF000000 | (r << 16) | (g << 8) | b;
        uint32_t* buffer = (uint32_t*)surface->buffer; 

        unsigned int maxHeight = font->lineHeight;

        if(y < 0 && static_cast<unsigned>(-y) > maxHeight){
            return 0;
        }

        if(y + static_cast<int>(maxHeight) >= limits.y + limits.height){
            maxHeight = limits.y + limits.height - y;
        }

        if(y + static_cast<int>(maxHeight) >= surface->height){
            maxHeight = surface->height - y;
        }

        int xOffset = x; 
        while (*str != 0) {
            if(*str == '\n'){
                break;
            } else if (!isprint(*str)) {
                str++;
                continue;
            }

            if(int err = FT_Load_Char(font->face, *str, FT_LOAD_RENDER)) {
                printf("Freetype Error (%d)\n", err);
                throw new FontException(FontException::FontRenderError, err);
                fontState = 0;
                return 0;
            }

            if(xOffset + (font->face->glyph->advance.x >> 6) < limits.x){
                xOffset += font->face->glyph->advance.x >> 6;
                str++;
                continue;
            }

            unsigned i = 0;
            if(y < 0){
                i = -y;
            }

            if(y + (font->height - font->face->glyph->bitmap_top) < limits.y){
                i = limits.y - (y + (font->height - font->face->glyph->bitmap_top));
            }

            for(; i < font->face->glyph->bitmap.rows && i + (font->height - font->face->glyph->bitmap_top) < maxHeight; i++){
                uint32_t yOffset = (i + y + (font->height - font->face->glyph->bitmap_top)) * (surface->width);
                
                unsigned j = 0;
                if(xOffset < 0){
                    j = -x;
                }

                if(xOffset < limits.x){
                    j = limits.x - xOffset;
                }

                for(; j < font->face->glyph->bitmap.width && (xOffset + static_cast<long>(j)) < surface->width; j++){
                    unsigned off = yOffset + (j + xOffset);
                    if(font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] == 255)
                        buffer[off] = colour_i;
                    else if( font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j]){
                        double val = font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] * 1.0 / 255;
                        buffer[off] = AlphaBlend(buffer[off], r, g, b, val);
                    }
                }
            }
            
            xOffset += font->face->glyph->advance.x >> 6;
            str++;
        }
        return xOffset - x;
    }

    int DrawString(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b, surface_t* surface, Font* font) {
        return DrawString(str, x, y, r, g, b, surface, {0, 0, surface->width, surface->height}, font);
    }

    int DrawString(const char* str, int x, int y, rgba_colour_t col, surface_t* surface, rect_t limits, Font* font){
        return DrawString(str, x, y, col.r, col.g, col.b, surface, limits, font);
    }

    int DrawString(const char* str, int x, int y, rgba_colour_t col, surface_t* surface, Font* font){
        return DrawString(str, x, y, col.r, col.g, col.b, surface, font);
    }
    
    int GetCharWidth(char c, Font* font){
        if((fontState != 1 && fontState != -1) || !font->face) InitializeFonts();
        if(fontState == -1){
            return 8;
        }
        
        if(c == '\n'){
            return 0;
        } else if (c == ' ') {
            return font->width;
        }  else if (c == '\t') {
            return font->tabWidth * font->width;
        } else if (!isprint(c)) {
            return 0;
        }

        if(int err = FT_Load_Char(font->face, c, FT_LOAD_ADVANCE_ONLY)) {
            printf("Freetype Error (%d)\n", err);
            throw new FontException(FontException::FontRenderError, err);
            fontState = 0;
            return 0;
        }

        return font->face->glyph->advance.x >> 6;
    }

    int GetCharWidth(char c){
        return GetCharWidth(c, mainFont);
    }

    int GetTextLength(const char* str, size_t n, Font* font){
        if(fontState != 1 && fontState != -1) InitializeFonts();
        if(fontState == -1){
            return strlen(str) * 8;
        }

        size_t len = 0;
        size_t i = 0;
        while (*str && i++ < n) {
            if(*str == '\n'){
                break;
            } else if (*str == ' ') {
                len += font->width;
                str++;
                continue;
            }  else if (*str == '\t') {
                len += font->tabWidth * font->width;
                str++;
                continue;
            } else if (!isprint(*str)) {
                str++;
                continue;
            }

            if(int err = FT_Load_Char(font->face, *str, FT_LOAD_ADVANCE_ONLY)) {
                printf("Freetype Error (%d)\n", err);
                throw new FontException(FontException::FontRenderError, err);
                fontState = 0;
                return 0;
            }

            len += font->face->glyph->advance.x >> 6;
            str++;
        }

        return len;
    }

    int GetTextLength(const char* str, Font* font){
        return GetTextLength(str, strlen(str), font);
    }

    int GetTextLength(const char* str, size_t n){
        return GetTextLength(str, n, mainFont);
    }

    int GetTextLength(const char* str){
        return GetTextLength(str, strlen(str));
    }

    TextObject::TextObject(vector2i_t pos, std::string& text, Font* font){
        this->pos = pos;
        this->text = text;
        this->font = font;

        CalculateSizes();
    }

    TextObject::TextObject(vector2i_t pos, const char* text, Font* font){
        this->pos = pos;
        this->text = text;
        this->font = font;

        CalculateSizes();
    }

    TextObject::TextObject(vector2i_t pos, Font* font){
        this->pos = pos;
        this->font = font;

        CalculateSizes();
    }

    void TextObject::CalculateSizes(){
        textSize.x = GetTextLength(text.c_str(), font); 
        textSize.y = font->height;
    }

    void TextObject::Render(surface_t* surface){
        switch(renderMode){
            case RenderNormal:
            default:
                DrawString(text.c_str(), pos.x, pos.y, colour, surface, font);
        }
    }
}
