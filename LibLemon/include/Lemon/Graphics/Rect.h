#pragma once

#include <Lemon/Graphics/Vector.h>

#include <list>

typedef struct Rect {
    union {
        Vector2i pos;
        struct {
            int x;
            int y;
        };
    };
    union {
        Vector2i size;
        struct {
            int width;
            int height;
        };
    };

    __attribute__((always_inline)) inline int left() const { return x; }
    __attribute__((always_inline)) inline int right() const { return x + width; }
    __attribute__((always_inline)) inline int top() const { return y; }
    __attribute__((always_inline)) inline int bottom() const { return y + height; }

    __attribute__((always_inline)) inline int left(int newLeft) {
        width += x - newLeft;
        x = newLeft;
        return x;
    }
    __attribute__((always_inline)) inline int right(int newRight) {
        width = newRight - x;
        return x + width;
    }
    __attribute__((always_inline)) inline int top(int newTop) {
        height += y - newTop;
        y = newTop;
        return y;
    }
    __attribute__((always_inline)) inline int bottom(int newBottom) {
        height = newBottom - y;
        return y + height;
    }

    std::list<Rect> Split(Rect cut) {
        std::list<Rect> clips;
        Rect victim = *this;

        if (cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(cut.left()); // Left of cutting rect's left edge

            victim.left(cut.left());

            clips.push_back(clip);
        }

        if (cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(cut.top()); // Above cutting rect's top edge
            clip.right(victim.right());

            victim.top(cut.top());

            clips.push_back(clip);
        }

        if (cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
            Rect clip;
            clip.top(victim.top());
            clip.left(cut.right());
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.right(cut.right());

            clips.push_back(clip);
        }

        if (cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
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

    inline bool Intersects(const Rect& other){
        return (left() < other.right() && right() > other.left() && top() < other.bottom() && bottom() > other.top());
    }
} rect_t; // Rectangle
