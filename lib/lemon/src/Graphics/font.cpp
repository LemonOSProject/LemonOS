#include <lemon/graphics/Font.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <string>

namespace Lemon::Graphics {
const char* FontException::errorStrings[] = {
    "Unknown Font Error",      "Failed to open font file", "Freetype error on loading font",
    "Error setting font size", "Error rendering font",
};

int fontState = 0;
Font* mainFont = nullptr;

static FT_Library library;
static std::unordered_map<std::string, Font*>* fonts =
    nullptr; // Clang appears to call InitializeFonts before the constructor for the map

void RefreshFonts() {
    if (library)
        FT_Done_FreeType(library);
    fontState = 0;
}

__attribute__((constructor)) void InitializeFonts() {
    fontState = -1;

    if (FT_Init_FreeType(&library)) {
        printf("Error initializing freetype");
        return;
    }

    fonts = new std::unordered_map<std::string, Font*>{};

    mainFont = LoadFont("/system/lemon/resources/fonts/notosans.otf", "default", 10);
    if(!mainFont) {
        return;
    }

    fontState = 1;
}

Font* LoadFont(const char* path, const char* id, int sz) {
    Font* font = new Font;
    FT_Face face;

    if (int err = FT_New_Face(library, path, 0, &face)) {
        // Freetype Error loading custom font from memory
        throw FontException(FontException::FontLoadError, err);
        return nullptr;
    }

    if (int err = FT_Set_Pixel_Sizes(face, 0, sz / 72.f * 96)) {
        // Freetype Error Setting Font Size
        throw FontException(FontException::FontSizeError, err);
        return nullptr;
    }

    if (int err = FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
        throw FontException(FontException::FontLoadError, err);
        return nullptr;
    }

    if (!id) {
        char buf[32];
        snprintf(buf, 32, "%s", path);
        font->id = new char[strlen(buf) + 1];
        strcpy(font->id, buf);
    } else {
        font->id = new char[strlen(id) + 1];
        strcpy(font->id, id);
    }

    font->pixelHeight = sz / 72.f * 96; // pt to px
    font->height = font->pixelHeight;
    font->lineHeight = face->size->metrics.height / 64;
    font->monospace = FT_IS_FIXED_WIDTH(face);
    font->width = 8;
    font->tabWidth = 4;

    font->face = face;

    assert(fonts);
    fonts->insert({font->id, font});

    return font;
}

Font* GetFont(const char* id) {
    Font* font;
    try {
        font = fonts->at(id);
    } catch (const std::out_of_range& e) {
        fprintf(stderr, "Warning: font %s could not be found!", id);
        font = mainFont;
    }
    return font;
}
}; // namespace Lemon::Graphics
