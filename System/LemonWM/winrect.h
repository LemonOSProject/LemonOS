#pragma once

#include <lemon/gfx/types.h>

class WMWindow;
struct WMWindowRect {
    WMWindow* win = nullptr;
    Rect rect;

    WMWindowRect() = default;

    WMWindowRect(const Rect& r){
        rect = r;
    }

    WMWindowRect(const Rect& r, WMWindow* w){
        rect = r;

        win = w;
    }

    __attribute__((always_inline)) inline int left() const { return rect.left(); }
    __attribute__((always_inline)) inline int right() const { return rect.right(); }
    __attribute__((always_inline)) inline int top() const { return rect.top(); }
    __attribute__((always_inline)) inline int bottom() const { return rect.bottom(); }

    __attribute__((always_inline)) inline int left(int newLeft) { return rect.left(newLeft); }
    __attribute__((always_inline)) inline int right(int newRight) { return rect.right(newRight); }
    __attribute__((always_inline)) inline int top(int newTop) { return rect.top(newTop); }
    __attribute__((always_inline)) inline int bottom(int newBottom) { return rect.bottom(newBottom); }

    operator Rect(){
        return rect;
    }

    std::list<WMWindowRect> Split(const Rect& cut){
        std::list<WMWindowRect> clips;
        WMWindowRect victim = *this;

        if(cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
            WMWindowRect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(cut.left()); // Left of cutting rect's left edge
            clip.win = win;

            victim.left(cut.left());

            clips.push_back(clip);
        }

        if(cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
            WMWindowRect clip;
            clip.top(victim.top());
            clip.left(victim.left());
            clip.bottom(cut.top()); // Above cutting rect's top edge
            clip.right(victim.right());
            clip.win = win;

            victim.top(cut.top());

            clips.push_back(clip);
        }

        if(cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
            WMWindowRect clip;
            clip.top(victim.top());
            clip.left(cut.right());
            clip.bottom(victim.bottom());
            clip.right(victim.right());
            clip.win = win;

            victim.right(cut.right());

            clips.push_back(clip);
        }

        if(cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
            WMWindowRect clip;

            clip.top(cut.bottom());
            clip.left(victim.left());
            clip.bottom(victim.bottom());
            clip.right(victim.right());
            clip.win = win;

            victim.bottom(cut.bottom());

            clips.push_back(clip);
        }

        return clips;
    }
};