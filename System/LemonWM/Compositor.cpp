#include "Compositor.h"

#include "WM.h"

#include <Lemon/Core/Logger.h>
#include <Lemon/Graphics/Graphics.h>

using namespace Lemon;

Compositor::Compositor(const Surface& displaySurface) : m_displaySurface(displaySurface) {
    // Create a backbuffer surface for rendering
    m_renderSurface = displaySurface;
    m_renderSurface.buffer = new uint8_t[m_displaySurface.width * m_displaySurface.height * 4];
}

void Compositor::Render() {
    if(m_invalidateAll){
        RecalculateWindowClipping();
        RecalculateBackgroundClipping();
    }

    for (const Rect& rect : m_backgroundRects) {
        Lemon::Graphics::DrawRectOutline(rect, {255, 0, 0, 255}, &m_renderSurface);
    }

    Vector2i mousePos = WM::Instance().m_input.mouse.pos;
    Lemon::Graphics::DrawRect({mousePos, {2, 2}}, {255, 0, 0, 255}, &m_renderSurface);

    // Copy the render surface to the display surface
    m_displaySurface.Blit(&m_renderSurface);

    Invalidate({mousePos, {2, 2}});
}

void Compositor::Invalidate(const Rect& rect) {}

void Compositor::InvalidateWindow(class WMWindow* window) {}

void Compositor::RecalculateWindowClipping() {

}

void Compositor::RecalculateBackgroundClipping() {
    m_backgroundRects.clear();

    m_backgroundRects.push_back(Rect{0, 0, m_displaySurface.width, m_displaySurface.height});

    auto& windows = WM::Instance().m_windows;
retry:
    for (auto it = m_backgroundRects.begin(); it != m_backgroundRects.end(); it++) {
        for (WMWindow* win : windows) {
            if (!(win->IsMinimized() || win->IsTransparent())) {
                if (it->Intersects(win->GetContentRect())) {
                    auto result = it->Split(win->GetContentRect());

                    m_backgroundRects.erase(it);
                    m_backgroundRects.splice(m_backgroundRects.end(), std::move(result));
                    goto retry;
                }
            }
        }
    }
}
