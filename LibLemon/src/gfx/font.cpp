#include <Lemon/Graphics/Font.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <cstring>

namespace Lemon::Graphics{
    const char* FontException::errorStrings[] = {
        "Unknown Font Error",
        "Failed to open font file",
        "Freetype error on loading font",
        "Error setting font size",
        "Error rendering font",
    };

    int fontState = 0;
    Font* mainFont = nullptr;
    
    static FT_Library library;
    static std::map<std::string, Font*> fonts = {{"default", mainFont}}; // Clang appears to call InitializeFonts before the constructor for the map

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

        mainFont = new Font;
        
        if(int err = FT_New_Face(library, "/initrd/montserrat.ttf", 0, &mainFont->face)){
            printf("Freetype Error (%d) loading font from memory /initrd/montserrat.ttf\n",err);
            return;
        }

        mainFont->height = 12;

        if(int err = FT_Set_Pixel_Sizes(mainFont->face, 0, mainFont->height)){
            printf("Freetype Error (%d) Setting Font Size\n", err);
            return;
        }

        mainFont->lineHeight = 16;
        mainFont->id = new char[strlen("default") + 1];
        mainFont->width = 8;
        mainFont->tabWidth = 4;
        strcpy(mainFont->id, "default");

        fonts["default"] = mainFont;

        fontState = 1;
    }

    Font* LoadFont(const char* path, const char* id, int sz){
        Font* font = new Font;

        if(int err = FT_New_Face(library, path, 0, &font->face)){
            // Freetype Error loading custom font from memory
            throw FontException(FontException::FontLoadError, err);
            return nullptr;
        }

        if(int err = FT_Set_Pixel_Sizes(font->face, 0, sz)){
            // Freetype Error Setting Font Size
            throw FontException(FontException::FontSizeError, err);
            return nullptr;
        }

        if(!id){
            char buf[32];
            sprintf(buf, "%s", path);
            font->id = new char[strlen(buf) + 1];
            strcpy(font->id, buf);
        } else {
            font->id = new char[strlen(id) + 1];
            strcpy(font->id, id);
        }

        font->height = sz;
        font->lineHeight = sz + sz / 3;
        font->monospace = FT_IS_FIXED_WIDTH(font->face);
        font->width = 8;
        font->tabWidth = 4;

        fonts[font->id] = font;

        fontState = 1;

        return font;
    }

    Font* GetFont(const char* id){
        Font* font;
        try{
            font = fonts.at(id);
        } catch (const std::out_of_range& e){
            fprintf(stderr, "Warning: font %s could not be found!", id);
            font = mainFont;
        }
        return font;
    }
};