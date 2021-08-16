#pragma once

#include <Lemon/Graphics/Surface.h>
#include <Lemon/Graphics/Types.h>

#include <list>
#include <thread>
#include <atomic>

#define COMPOSITOR_DEBUG 1

template<typename T, class D>
std::list<T> Split(Rect victim, const Rect& cut, D extraData) {
    std::list<T> clips;

    if (cut.left() >= victim.left() && cut.left() <= victim.right()) { // Clip left edge
        Rect clip;
        clip.top(victim.top());
        clip.left(victim.left());
        clip.bottom(victim.bottom());
        clip.right(cut.left()); // Left of cutting rect's left edge

        victim.left(cut.left());

        clips.push_back({clip, extraData});
    }

    if (cut.top() >= victim.top() && cut.top() <= victim.bottom()) { // Clip top edge
        Rect clip;
        clip.top(victim.top());
        clip.left(victim.left());
        clip.bottom(cut.top()); // Above cutting rect's top edge
        clip.right(victim.right());

        victim.top(cut.top());

        clips.push_back({clip, extraData});
    }

    if (cut.right() >= victim.left() && cut.right() <= victim.right()) { // Clip right edge
        Rect clip;
        clip.top(victim.top());
        clip.left(cut.right());
        clip.bottom(victim.bottom());
        clip.right(victim.right());

        victim.right(cut.right());

        clips.push_back({clip, extraData});
    }

    if (cut.bottom() >= victim.top() && cut.bottom() <= victim.bottom()) { // Clip bottom edge
        Rect clip;
        clip.top(cut.bottom());
        clip.left(victim.left());
        clip.bottom(victim.bottom());
        clip.right(victim.right());

        victim.bottom(cut.bottom());

        clips.push_back({clip, extraData});
    }

    return clips;
}

struct WindowClipRect {
    Rect rect;
    class WMWindow* win;

    std::list<WindowClipRect> Split(const Rect& cut) {
        return ::Split<WindowClipRect, WMWindow*>(rect, cut, win);
    }
};

struct BackgroundClipRect {
    Rect rect;
    bool invalid = true;

    std::list<BackgroundClipRect> Split(const Rect& cut) {
        return ::Split<BackgroundClipRect, bool>(rect, cut, true);
    }
};

class Compositor {
public:
    Compositor(const Surface& displaySurface);

    void Render();

    inline Vector2i GetScreenBounds() const { return { m_renderSurface.width, m_renderSurface.height }; }

    inline void InvalidateAll() { m_invalidateAll = true; }
    void Invalidate(const Rect& rect);
    void InvalidateWindow(class WMWindow* window);

    void SetWallpaper(const std::string& path);

private:
    void RecalculateWindowClipping();
    void RecalculateBackgroundClipping();

    bool m_invalidateAll = true;

    Surface m_renderSurface;  // Backbuffer to render to
    Surface m_displaySurface; // Display mapped surface

    Surface m_wallpaper = {}; // Wallpaper surface
    std::thread m_wallpaperThread;
    std::atomic<int> m_wallpaperStatus = 0;

    std::list<BackgroundClipRect> m_backgroundRects;
    std::list<WindowClipRect> m_windowClipRects;
};
