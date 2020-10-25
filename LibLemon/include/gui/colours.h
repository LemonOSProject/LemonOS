#pragma once

#include <gfx/graphics.h>

namespace Lemon{
    enum Colour{
        Background, // Window Background, etc.
        ContentBackground, // Textbox Background, etc.
        Text, // Labels, TextBox, Some Button Styles
        TextAlternate, // Other Button Styles
        Foreground,
        ContentShadow,
    };

    extern RGBAColour colours[];
}