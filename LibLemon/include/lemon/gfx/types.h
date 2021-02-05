#pragma once

#include <list>

typedef struct Vector2i{
    int x, y;
} vector2i_t; // Two dimensional integer vector

inline vector2i_t operator+ (const vector2i_t& l, const vector2i_t& r){
    return {l.x + r.x, l.y + r.y};
}

inline void operator+= (vector2i_t& l, const vector2i_t& r){
    l = l + r;
}

inline vector2i_t operator- (const vector2i_t& l, const vector2i_t& r){
    return {l.x - r.x, l.y - r.y};
}

inline void operator-= (vector2i_t& l, const vector2i_t& r){
    l = l - r;
}

inline bool operator== (const vector2i_t& l, const vector2i_t& r){
    return l.x == r.x && l.y == r.y;
}

inline bool operator!= (const vector2i_t& l, const vector2i_t& r){
    return l.x != r.x && l.y != r.y;
}

typedef struct Rect{
    union {
        vector2i_t pos;
        struct {
            int x;
            int y;
        };
    };
    union {
        vector2i_t size;
        struct {
            int width;
            int height;
        };
    };

    __attribute__((always_inline)) inline int left() const { return x; }
    __attribute__((always_inline)) inline int right() const { return x + width; }
    __attribute__((always_inline)) inline int top() const { return y; }
    __attribute__((always_inline)) inline int bottom() const { return y + height; }

    __attribute__((always_inline)) inline int left(int newLeft) { width += x - newLeft; x = newLeft; return x; }
    __attribute__((always_inline)) inline int right(int newRight) { width = newRight - x; return x + width; }
    __attribute__((always_inline)) inline int top(int newTop) { height += y - newTop; y = newTop; return y; }
    __attribute__((always_inline)) inline int bottom(int newBottom) { height = newBottom - y; return y + height; }

    std::list<Rect> Split(Rect cut){
        std::list<Rect> clips;
        Rect victim = *this;

        if(cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(cut.left()); // Left of cutting rect's left edge

            victim.left(cut.left());

            clips.push_back(clip);
        }

        if(cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(cut.top()); // Above cutting rect's top edge
            clip.right(victim.right());

            victim.top(cut.top());

            clips.push_back(clip);
        }

        if(cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
            Rect clip;
            clip.top(victim.top());
            clip.left(cut.right());
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.right(cut.right());

            clips.push_back(clip);
        }

        if(cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
            Rect clip;
            clip.top(cut.bottom());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.bottom(cut.bottom());

            clips.push_back(clip);
        }

        return clips;
    }
} rect_t; // Rectangle

typedef struct RGBAColour{
    union{
        struct{
            uint8_t r, g, b, a; /* Red, Green, Blue, Alpha (Transparency) Respectively*/
        };
        uint32_t val;
    };

    inline static constexpr const RGBAColour FromRGB(uint32_t rgb){
        return { .val = __builtin_bswap32((rgb << 8) | 0xff)}; // Set alpha channel to 255
    }

    inline static constexpr const RGBAColour FromARGB(uint32_t argb){
        return { .val = __builtin_bswap32((argb << 8) | ((argb >> 24) & 0xff))}; // Swap alpha
    }
} __attribute__ ((packed)) rgba_colour_t;