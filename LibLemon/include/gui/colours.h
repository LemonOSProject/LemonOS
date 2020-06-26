#pragma once

#include <gfx/graphics.h>

namespace Lemon{
    enum Colour{
        Background, // Window Background, etc.
        ContentBackground, // Textbox Background, etc.
        TextDark, // Labels, TextBox, Some Button Styles
        TextLight, // Other Button Styles
        Foreground,
        ContentShadow,
    };

    extern RGBAColour colours[];
}