#include <Lemon/GUI/Colours.h>

namespace Lemon{
    /*RGBAColour colours[] = {
        {220, 220, 210, 255},
        {255, 255, 255, 255},
        {0,   0,   0,   255},
        {255, 255, 250, 255},
        {184, 96, 22,  255},
        {190, 190, 180, 255},
    };*/

    RGBAColour colours[] = {
        RGBAColour::FromRGB(0x22211f),
        RGBAColour::FromRGB(0x32312d),
        {239, 241, 243, 255},
        {0, 0, 0, 255},
        {184, 96, 22,  255},
        RGBAColour::Interpolate({184, 96, 22,  255}, RGBAColour::FromRGB(0x242322)),
        RGBAColour::FromRGB(0x2f2e2c),
    };
}