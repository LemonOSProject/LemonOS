#include <gfx/font.h>

#include <stdint.h>
#include <list.h>

namespace Lemon::Graphics{
    const char* FontException::errorStrings[] = {
        "Unknown Font Error",
        "Failed to open font file",
        "Freetype error on loading font",
        "Error setting font size",
    };

    int fontState = 0;
    Font* mainFont = nullptr;
    
    static FT_Library library;
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

        mainFont = new Font;
        
        if(int err = FT_New_Face(library, "/initrd/montserrat.ttf", 0, &mainFont->face)){
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
};