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
    if (m_wallpaperThread.joinable() && m_wallpaperStatus) {
        m_wallpaperThread.join();
    }

    if (m_invalidateAll) {
        RecalculateWindowClipping();
        RecalculateBackgroundClipping();
    }

    if (m_wallpaper.buffer) {
        for (const BackgroundClipRect& rect : m_backgroundRects) {
            /*if (!rect.invalid) {
                return;
            }*/
            
            m_renderSurface.Blit(&m_wallpaper, rect.rect.pos, rect.rect);

#ifdef COMPOSITOR_DEBUG
            Lemon::Graphics::DrawRectOutline(rect.rect, {255, 0, 0, 255}, &m_renderSurface);
#endif
        }
    }
    
    for (const WindowClipRect& rect : m_windowClipRects) {
            rect.win->DrawClip(rect.rect, &m_renderSurface);

#ifdef COMPOSITOR_DEBUG
            //Lemon::Graphics::DrawRect(rect.rect, {255, 0, 255, 255}, &m_displaySurface);
            Lemon::Graphics::DrawRectOutline(rect.rect, {0, 0, 255, 255}, &m_renderSurface);
#endif
    }

    Vector2i mousePos = WM::Instance().m_input.mouse.pos;
    Lemon::Graphics::DrawRect({mousePos, {5, 5}}, {0, 255, 0, 255}, &m_renderSurface);

    // Copy the render surface to the display surface
    m_displaySurface.Blit(&m_renderSurface);

    Invalidate({mousePos, {5, 5}});
}

void Compositor::Invalidate(const Rect& rect) {
    if (m_invalidateAll) {
        return;
    }

    for(auto& bgRect : m_backgroundRects) {
        if(bgRect.rect.Intersects(rect)){
            bgRect.invalid = true; // Set bg rect as invalid
        }
    }
}

void Compositor::InvalidateWindow(class WMWindow* window) {
    if (m_invalidateAll) {
        return;
    }
}

void Compositor::SetWallpaper(const std::string& path) {
    m_wallpaperStatus = 0;
    std::thread wallpaperThread([this, path]() -> void {
        if (Graphics::LoadImage(path.c_str(), 0, 0, m_displaySurface.width, m_displaySurface.height, &m_wallpaper,
                                true)) {
            Logger::Error("Failed to load wallpaper '", path, "'");
        }

        m_wallpaperStatus++;
    });

    m_wallpaperThread = std::move(wallpaperThread);
}

void Compositor::RecalculateWindowClipping() {
    m_windowClipRects.clear();

    for (WMWindow* win : WM::Instance().m_windows) {
        if (win->IsMinimized()) {
            continue;
        } else if (win->IsTransparent()) {
            // Transparent windows do not cut other windows
            m_windowClipRects.push_back({win->GetRect(), win});
            continue;
        }

    retry:
        for (auto it = m_windowClipRects.begin(); it != m_windowClipRects.end(); it++) {
            if (it->rect.Intersects(win->GetContentRect())) {
                m_windowClipRects.erase(it);

                // Only use the window content to clip other windows
                // This is so the windows render under the rounded corners
                m_windowClipRects.splice(m_windowClipRects.end(), it->Split(win->GetContentRect()));
                goto retry;
            }
        }

        m_windowClipRects.push_back({win->GetRect(), win});
    }
}

void Compositor::RecalculateBackgroundClipping() {
    m_backgroundRects.clear();

    m_backgroundRects.push_back({Rect{0, 0, m_displaySurface.width, m_displaySurface.height}, true});

    auto& windows = WM::Instance().m_windows;
retry:
    for (auto it = m_backgroundRects.begin(); it != m_backgroundRects.end(); it++) {
        for (WMWindow* win : windows) {
            if (!(win->IsMinimized() || win->IsTransparent())) {
                if (it->rect.Intersects(win->GetContentRect())) {
                    auto result = it->Split(win->GetContentRect());

                    m_backgroundRects.erase(it);
                    m_backgroundRects.splice(m_backgroundRects.end(), std::move(result));
                    goto retry;
                }
            }
        }
    }
}
