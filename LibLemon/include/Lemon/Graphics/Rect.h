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
    __attribute__((always_inline)) inline int right() const { return x + width - 1; }
    __attribute__((always_inline)) inline int top() const { return y; }
    __attribute__((always_inline)) inline int bottom() const { return y + height - 1; }

    __attribute__((always_inline)) inline int left(int newLeft) {
        width += x - newLeft;
        x = newLeft;
        return x;
    }
    __attribute__((always_inline)) inline int right(int newRight) {
        width = newRight - x + 1;
        return x + width;
    }
    __attribute__((always_inline)) inline int top(int newTop) {
        height += y - newTop;
        y = newTop;
        return y;
    }
    __attribute__((always_inline)) inline int bottom(int newBottom) {
        height = newBottom - y + 1;
        return y + height;
    }

    std::list<Rect> Split(const Rect& cut) const {
        std::list<Rect> clips;
        Rect victim = *this;

        if (cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(cut.left() - 1); // Left of cutting rect's left edge

            victim.left(cut.left());

            clips.push_back(clip);
        }

        if (cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
            Rect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(cut.top() - 1); // Above cutting rect's top edge
            clip.right(victim.right());

            victim.top(cut.top());

            clips.push_back(clip);
        }

        if (cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
            Rect clip;
            clip.top(victim.top());
            clip.left(cut.right() + 1);
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.right(cut.right());

            clips.push_back(clip);
        }

        if (cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
            Rect clip;
            clip.top(cut.bottom() + 1);
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(victim.right());

            victim.bottom(cut.bottom());

            clips.push_back(clip);
        }

        return clips;
    }

    inline bool Intersects(const Rect& other) const {
        return (left() < other.right() && right() > other.left() && top() < other.bottom() && bottom() > other.top());
    }

    // Get overlap of two rectangles
    inline Rect GetIntersect(const Rect& cut) const {
        Rect victim = *this;

        if (cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
            victim.left(cut.left());
        }

        if (cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
            victim.top(cut.top());
        }

        if (cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
            victim.right(cut.right());
        }

        if (cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
            victim.bottom(cut.bottom());
        }

        return victim;
    }

    inline bool Contains(const Rect& other) const {
        return (left() < other.right() && left() <= other.left() && right() > other.left() && right() >= other.right() && top() < other.bottom() && top() <= other.top() && bottom() > other.top() && bottom() >= other.bottom());
    }

    inline bool Contains(const Vector2i& other) const {
        return (other.x >= x && other.x < right() && other.y >= y && other.y < bottom());
    }
} rect_t; // Rectangle
