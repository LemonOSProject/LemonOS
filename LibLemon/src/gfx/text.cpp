#include <gfx/graphics.h>

#include <gfx/font.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <ctype.h>
#include <list.h>

extern uint8_t font_default[];

namespace Lemon::Graphics{
    static int fontState = 0;
    static FT_Library library;
    static Font* mainFont = nullptr;
    static List<Font*> fonts;

    void RefreshFonts(){
        if(library) FT_Done_FreeType(library);
        fontState = 0;
    }

    __attribute__((constructor))
    void InitializeFonts(){
        fontState = -1;

        if(FT_Init_FreeType(&library)){
            printf("Error initializing freetype");
            return;
        }

        FILE* fontFile = fopen("/initrd/montserrat.ttf", "r");
        
        if(!fontFile){
            printf("Error loading font /initrd/montserrat.ttf");
            return;
        }

        fseek(fontFile, 0, SEEK_END);
        size_t fontSize = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);
        uint8_t* fontBuffer = (uint8_t*)malloc(fontSize);
        fread(fontBuffer, fontSize, 1, fontFile);

        fclose(fontFile);

        mainFont = new Font;

        if(int err = FT_New_Memory_Face(library, fontBuffer, fontSize, 0, &mainFont->face)){
            printf("Freetype Error (%d) loading font from memory /initrd/montserrat.ttf\n",err);
            return;
        }

        if(int err = FT_Set_Pixel_Sizes(mainFont->face, 0, 12)){
            printf("Freetype Error (%d) Setting Font Size\n", err);
            return;
        }

        mainFont->id = new char[strlen("default") + 1];
        mainFont->height = 12;
        mainFont->width = 8;
        mainFont->tabWidth = 4;
        strcpy(mainFont->id, "default");

        fonts.add_back(mainFont);

        fontState = 1;
    }

    Font* LoadFont(const char* path, const char* id, int sz){
        fontState = -1;

        if(FT_Init_FreeType(&library)){
            //syscall(0,(uintptr_t)"Error initializing freetype",0,0,0,0);
            return nullptr;
        }

        FILE* fontFile = fopen(path, "r");
        
        if(!fontFile){
            //syscall(0,(uintptr_t)"Error loading custom font",0,0,0,0);
            return nullptr;
        }

        fseek(fontFile, 0, SEEK_END);
        size_t fontSize = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);
        uint8_t* fontBuffer = (uint8_t*)malloc(fontSize);
        fread(fontBuffer, fontSize, 1, fontFile);

        fclose(fontFile);

        Font* font = new Font;

        if(int err = FT_New_Memory_Face(library, fontBuffer, fontSize, 0, &font->face)){
            printf("Freetype Error (%d) loading custom font from memory\n",err);
            return nullptr;
        }

        if(int err = FT_Set_Pixel_Sizes(font->face, 0, sz)){
            printf("Freetype Error (%d) Setting Font Size\n", err);
            return nullptr;
        }

        if(!id){
            char* buf = new char[24];
            sprintf(buf, "font%d", fonts.get_length());
            font->id = new char[strlen(buf) + 1];
            strcpy(font->id, buf);
            delete buf;
        } else {
            font->id = new char[strlen(id) + 1];
            strcpy(font->id, id);
        }

        font->height = sz;
        font->monospace = FT_IS_FIXED_WIDTH(font->face);
        font->width = 8;
        font->tabWidth = 4;

        fonts.add_back(font);

        fontState = 1;

        return font;
    }

    Font* GetFont(const char* id){
        for(unsigned i = 0; i < fonts.get_length(); i++){
            if(strcmp(fonts[i]->id, id) == 0) return fonts[i];
        }
        return mainFont;
    }

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
            printf("Freetype Error (%d)\n", err);
            fontState = 0;
            return 0;
        }

        int maxHeight = font->height - (font->height - font->face->glyph->bitmap_top);

        if(y + maxHeight >= limits.y + limits.height){
            maxHeight = limits.y + limits.height - y;
        }

        if(y + maxHeight >= surface->height){
            maxHeight = surface->height - y;
        }
        
        for(int i = 0; i < static_cast<int>(font->face->glyph->bitmap.rows) && i < maxHeight; i++){
            if(y + i < 0) continue;

            uint32_t yOffset = (i + y + (font->height - font->face->glyph->bitmap_top)) * (surface->width);
            for(unsigned int j = 0; j < font->face->glyph->bitmap.width && static_cast<long>(j) + x < surface->width; j++){
                if(x + static_cast<long>(j) < 0) continue;
                if(font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] == 255)
                    buffer[yOffset + (j + x)] = colour_i;
                else if(font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] > 0){
                    double val = font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] * 1.0 / 255;
                    uint32_t oldColour = buffer[yOffset + (j + x)];
                    int oldB = oldColour & 0xFF;
                    int oldG = (oldColour >> 8) & 0xFF;
                    int oldR = (oldColour >> 16) & 0xFF;
                    uint32_t newColour = (int)(b * val + oldB * (1 - val)) | (((int)(g * val + oldG * (1 - val)) << 8)) | (((int)(r * val + oldR * (1 - val)) << 16));
                    buffer[yOffset + (j + x)] = newColour;
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

        unsigned int maxHeight = font->height;

        if(y < 0 && static_cast<unsigned>(-y) > maxHeight){
            return 0;
        }

        if(y + static_cast<int>(maxHeight) >= limits.y + limits.height){
            maxHeight = limits.y + limits.height - y;
        }

        if(y + static_cast<int>(maxHeight) >= surface->height){
            maxHeight = surface->height - y;
        }

        int xOffset = 0;
        while (*str != 0) {
            if(*str == '\n'){
                break;
            } else if (!isprint(*str)) {
                str++;
                continue;
            }

            if(int err = FT_Load_Char(font->face, *str, FT_LOAD_RENDER)) {
                printf("Freetype Error (%d)\n", err);
                fontState = 0;
                return 0;
            }

            unsigned i = 0;
            if(y < 0){
                i = -y;
            }

            for(; i < font->face->glyph->bitmap.rows && i + (font->height - font->face->glyph->bitmap_top) < maxHeight; i++){
                uint32_t yOffset = (i + y + (font->height - font->face->glyph->bitmap_top)) * (surface->width);
                
                unsigned j = 0;
                if(x < 0){
                    j = -x;
                }

                for(; j < font->face->glyph->bitmap.width && (x + xOffset + static_cast<long>(j)) < surface->width; j++){
                    unsigned off = yOffset + (j + x + xOffset);
                    if(font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] == 255)
                        buffer[off] = colour_i;
                    else if( font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j]){
                        double val = font->face->glyph->bitmap.buffer[i * font->face->glyph->bitmap.width + j] * 1.0 / 255;
                        uint32_t oldColour = buffer[off];
                        int oldB = oldColour & 0xFF;
                        int oldG = (oldColour >> 8) & 0xFF;
                        int oldR = (oldColour >> 16) & 0xFF;
                        uint32_t newColour = (int)(b * val + oldB * (1 - val)) | (((int)(g * val + oldG * (1 - val)) << 8)) | (((int)(r * val + oldR * (1 - val)) << 16));
                        buffer[off] = newColour;
                    }
                }
            }
            
            xOffset += font->face->glyph->advance.x >> 6;
            str++;
        }
        return xOffset;
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

        if (!isprint(c)) {
            return 0;
        }

        if(int err = FT_Load_Char(font->face, c, FT_LOAD_ADVANCE_ONLY)) {
            printf("Freetype Error (%d)\n", err);
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
                len += font->tabWidth;
                str++;
                continue;
            } else if (!isprint(*str)) {
                str++;
                continue;
            }

            if(int err = FT_Load_Char(font->face, *str, FT_LOAD_ADVANCE_ONLY)) {
                printf("Freetype Error (%d)\n", err);
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
}
