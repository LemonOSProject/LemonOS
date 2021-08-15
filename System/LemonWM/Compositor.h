#pragma once

#include <Lemon/Graphics/Surface.h>
#include <Lemon/Graphics/Types.h>

#include <list>

class Compositor {
public:
    Compositor(const Surface& displaySurface);

    void Render();

    inline Vector2i GetScreenBounds() const { return { m_renderSurface.width, m_renderSurface.height }; }

    inline void InvalidateAll() { m_invalidateAll = true; }
    void Invalidate(const Rect& rect);
    void InvalidateWindow(class WMWindow* window);

private:
    void RecalculateWindowClipping();
    void RecalculateBackgroundClipping();

    bool m_invalidateAll = true;

    Surface m_renderSurface;  // Backbuffer to render to
    Surface m_displaySurface; // Display mapped surface

    Surface m_wallpaper = {}; // Wallpaper surface

    std::list<Rect> m_backgroundRects;
};
