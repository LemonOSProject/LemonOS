#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Lemon::Graphics{
    struct Font{
        bool monospace = false;
        FT_Face face;
        int height;
        int width;
        int tabWidth = 4;
        char* id;
    };
}
