#pragma once

#include <stdint.h>
#include <string>

typedef struct RGBAColour {
    union {
        struct {
            uint8_t r, g, b, a; /* Red, Green, Blue, Alpha (Transparency) Respectively*/
        } __attribute__((packed));
        uint32_t val;
    };

    inline static constexpr RGBAColour FromRGB(uint32_t rgb) {
        return {.val = __builtin_bswap32((rgb << 8) | 0xff)}; // Set alpha channel to 255
    }

    inline static constexpr RGBAColour FromARGB(uint32_t argb) {
        return {.val = __builtin_bswap32((argb << 8) | ((argb >> 24) & 0xff))}; // Swap alpha and byte order
    }

    inline static constexpr uint32_t ToARGB(const RGBAColour& colour) {
        return __builtin_bswap32(colour.a | (colour.val << 8)); // Swap alpha and byte order
    }

    inline static constexpr uint32_t ToARGBBigEndian(const RGBAColour& colour) {
        return colour.a | (colour.val << 8); // Swap alpha
    }

    inline static constexpr RGBAColour Interpolate(const RGBAColour& l, const RGBAColour& r) {
        return RGBAColour{.r = static_cast<uint8_t>((l.r + r.r) / 2),
                          .g = static_cast<uint8_t>((l.g + r.g) / 2),
                          .b = static_cast<uint8_t>((l.b + r.b) / 2),
                          .a = static_cast<uint8_t>((l.a + r.a) / 2)};
    }

    static const RGBAColour red;
    static const RGBAColour green;
    static const RGBAColour blue;
    static const RGBAColour yellow;
    static const RGBAColour cyan;
    static const RGBAColour magenta;
    static const RGBAColour black;
    static const RGBAColour white;
    static const RGBAColour grey;
    static const RGBAColour transparent;
} __attribute__((packed)) rgba_colour_t;

using Colour = RGBAColour;
