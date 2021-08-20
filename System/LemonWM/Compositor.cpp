#include "Compositor.h"

#include "WM.h"

#include <Lemon/Core/Logger.h>
#include <Lemon/Graphics/Graphics.h>

using namespace Lemon;

Compositor::Compositor(const Surface& displaySurface) : m_displaySurface(displaySurface) {
    // Create a backbuffer surface for rendering
    m_renderSurface = displaySurface;
    m_wallpaper = displaySurface;
    m_renderSurface.buffer = new uint8_t[m_displaySurface.width * m_displaySurface.height * 4];
    m_wallpaper.buffer = new uint8_t[m_displaySurface.width * m_displaySurface.height * 4];

    // Fallback background colour if wallpaper does not load
    Graphics::DrawRect(0, 0, m_wallpaper.width, m_wallpaper.height, {64, 128, 128, 255}, &m_wallpaper);

    if (Graphics::LoadImage("/system/lemon/resources/cursors/mouse.png", &m_cursorNormal)) {
        Logger::Debug("Error loading mouse cursor image!");
    }

    if (Graphics::LoadImage("/system/lemon/resources/cursors/resize.png", &m_cursorResize)) {
        Logger::Debug("Error loading resize cursor image!");
        m_cursorResize = m_cursorNormal;
    }
}

void Compositor::Render() {
    if (m_displayFramerate) {
        timespec cTime;
        clock_gettime(CLOCK_BOOTTIME, &cTime);

        unsigned long renderTime =
            (cTime.tv_nsec - m_lastRender.tv_nsec) + (cTime.tv_sec - m_lastRender.tv_sec) * 1000000000;

        m_avgFrametime += renderTime;

        if (m_avgFrametime > 1000000000) {
            if (m_avgFrametime)
                m_fRate = 1000000000 / (m_avgFrametime / m_fCount);
            m_fCount = 0;
            m_avgFrametime = renderTime;
        }

        m_fCount++;
        m_lastRender = cTime;
    }

    if (m_wallpaperThread.joinable() && m_wallpaperStatus) {
        m_wallpaperThread.join();
    }

    m_renderMutex.lock();

    Vector2i mousePos = WM::Instance().Input().mouse.pos;
    Rect mouseRect = {mousePos, {m_cursorCurrent->width, m_cursorCurrent->height}};
    if (m_lastMouseRect.pos != mousePos || m_lastMouseRect.size != mouseRect.size) {
        Invalidate(m_lastMouseRect);
        m_lastMouseRect = mouseRect;
    }

    if (m_invalidateAll) {
        RecalculateWindowClipping();
        RecalculateBackgroundClipping();

        // We fill the areas of the screen being redrawn in debug mode
        // This happens before the render surface is blitted to the display surface
#ifdef COMPOSITOR_DEBUG
        Lemon::Graphics::DrawRect(0, 0, m_displaySurface.width, m_displaySurface.height, {255, 0, 0, 255},
                                  &m_displaySurface);
#endif

        if (m_wallpaper.buffer) {
            for (BackgroundClipRect& rect : m_backgroundRects) {
                m_renderSurface.Blit(&m_wallpaper, rect.rect.pos, rect.rect);

#ifdef COMPOSITOR_DEBUG
                Lemon::Graphics::DrawRectOutline(rect.rect, {255, 255, 0, 255}, &m_renderSurface);
#endif

                rect.invalid = false;
            }
        }
    } else if (m_wallpaper.buffer) {
        for (BackgroundClipRect& rect : m_backgroundRects) {
            if (!rect.invalid) {
                continue;
            }

            m_renderSurface.Blit(&m_wallpaper, rect.rect.pos, rect.rect);

#ifdef COMPOSITOR_DEBUG
            Lemon::Graphics::DrawRectOutline(rect.rect, {255, 0, 0, 255}, &m_renderSurface);
#endif

            rect.invalid = false;
        }
    }

    for (auto it = m_windowClipRects.begin(); it != m_windowClipRects.end();) {
        WMWindow* win = it->win;

        if (win->IsDirtyAndClear()) { // Window buffer is dirty, draw all clips
            while (it != m_windowClipRects.end() && it->win == win) {
                win->DrawClip(it->rect, &m_renderSurface);

                it->invalid = false;
                it++;
            }

#ifdef COMPOSITOR_DEBUG
            Lemon::Graphics::DrawRect(it->rect, {255, 0, 255, 255}, &m_displaySurface);
            Lemon::Graphics::DrawRectOutline(it->rect, {0, 0, 255, 255}, &m_renderSurface);
#endif
        } else if (m_invalidateAll || it->invalid) { // Window buffer not dirty, only draw invalid clips
            win->DrawClip(it->rect, &m_renderSurface);

            it->invalid = false;
            it++;

#ifdef COMPOSITOR_DEBUG
            Lemon::Graphics::DrawRect(it->rect, {255, 0, 0, 255}, &m_displaySurface);
            Lemon::Graphics::DrawRectOutline(it->rect, {0, 0, 255, 255}, &m_renderSurface);
#endif
        } else {
            it++;
        }
    }

    for (WindowClipRect& rect : m_windowDecorationClipRects) {
        if (!(m_invalidateAll || rect.invalid)) {
            continue;
        }

        rect.win->DrawDecorationClip(rect.rect, &m_renderSurface);

#ifdef COMPOSITOR_DEBUG
        Lemon::Graphics::DrawRect(rect.rect, {0, 255, 0, 255}, &m_displaySurface);
        Lemon::Graphics::DrawRectOutline(rect.rect, {0, 255, 0, 255}, &m_renderSurface);
#endif

        rect.invalid = false;
    }

    m_renderSurface.AlphaBlit(m_cursorCurrent, mousePos);

    if (m_displayFramerate) {
        Lemon::Graphics::DrawRect(0, 0, 80, 18, 0, 0, 0, &m_renderSurface);
        Lemon::Graphics::DrawString(std::to_string(m_fRate).c_str(), 0, 0, 255, 255, 255, &m_renderSurface);
    }

    // Copy the render surface to the display surface
    m_displaySurface.Blit(&m_renderSurface);
    m_invalidateAll = false;

    m_renderMutex.unlock();
}

void Compositor::InvalidateAll() { m_invalidateAll = true; }

void Compositor::Invalidate(const Rect& rect) {
    if (m_invalidateAll) {
        return;
    }

    auto invalidateDecorationRect = [this](WindowClipRect& dRect, const Rect& rect) {
        if (dRect.invalid) {
            return;
        }

        if (dRect.rect.Intersects(rect)) {
            dRect.invalid = true; // Set bg rect as invalid

            // Make sure any background clips are redrawn
            for (auto& bgRect : m_backgroundRects) {
                if (bgRect.invalid) {
                    continue;
                }

                if (bgRect.rect.Intersects(dRect.rect)) {
                    bgRect.invalid = true; // Set bg rect as invalid
                }
            }

            // Make sure any window clips are redrawn
            for (auto& wRect : m_windowClipRects) {
                if (wRect.invalid) {
                    continue;
                }

                if (wRect.rect.Intersects(dRect.rect)) {
                    wRect.invalid = true; // Set bg rect as invalid
                }
            }
        }
    };

    for (auto& bgRect : m_backgroundRects) {
        if (bgRect.invalid) {
            continue;
        }

        if (bgRect.rect.Intersects(rect)) {
            bgRect.invalid = true; // Set bg rect as invalid
        }

        // If a decoration rect got clipped by a window,
        // The background clip may extend beyond the clip
        // To avoid drawing the background over the decorations
        // Mark any intersecting window decorations as invalid
        for (auto& dRect : m_windowDecorationClipRects) {
            invalidateDecorationRect(dRect, bgRect.rect);
        }
    }

    for (auto& wRect : m_windowClipRects) {
        if (wRect.invalid) {
            continue;
        }

        if (wRect.rect.Intersects(rect)) {
            wRect.invalid = true;
        }
    }

    for (auto& dRect : m_windowDecorationClipRects) {
        invalidateDecorationRect(dRect, rect);
    }
}

void Compositor::InvalidateWindow(class WMWindow* window) { m_invalidateAll = true; }

void Compositor::SetWallpaper(const std::string& path) {
    m_wallpaperStatus = 0;
    std::thread wallpaperThread([this, path]() -> void {
        std::unique_lock lock(m_wallpaperMutex);

        if (Graphics::LoadImage(path.c_str(), 0, 0, m_displaySurface.width, m_displaySurface.height, &m_wallpaper,
                                true)) {
            Logger::Error("Failed to load wallpaper '", path, "'");
        }

        m_renderMutex.lock();
        InvalidateAll();
        m_renderMutex.unlock();
        m_wallpaperStatus++;
    });

    m_wallpaperThread = std::move(wallpaperThread);
}

void Compositor::RecalculateWindowClipping() {
    m_windowClipRects.clear();
    m_windowDecorationClipRects.clear();

    for (WMWindow* win : WM::Instance().m_windows) {
        if (win->IsMinimized()) {
            continue;
        } else if (win->IsTransparent()) {
            // Transparent windows do not cut other windows
            m_windowClipRects.push_back({win->GetContentRect(), win});
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

        m_windowClipRects.push_back({win->GetContentRect(), win});

    retryDecoration:
        if (win->ShouldDrawDecoration()) {
            for (auto it = m_windowDecorationClipRects.begin(); it != m_windowDecorationClipRects.end(); it++) {
                if (it->rect.Intersects(win->GetRect())) {
                    m_windowDecorationClipRects.erase(it);

                    // Only use the window content to clip other windows
                    // This is so the windows render under the rounded corners
                    m_windowDecorationClipRects.splice(m_windowDecorationClipRects.end(), it->Split(win->GetRect()));
                    goto retryDecoration;
                }
            }

            m_windowDecorationClipRects.push_back({win->GetRect(), win});
        }
    }
}

void Compositor::RecalculateBackgroundClipping() {
    m_backgroundRects.clear();

    m_backgroundRects.push_back({Rect{0, 0, m_displaySurface.width, m_displaySurface.height}, true});

    auto& windows = WM::Instance().m_windows;
    for (WMWindow* win : windows) {
        if (win->IsMinimized() || win->IsTransparent()) {
            continue;
        }
    retry:
        for (auto it = m_backgroundRects.begin(); it != m_backgroundRects.end(); it++) {
            if (it->rect.Intersects(win->GetRect())) {
                auto result = it->Split(win->GetRect());

                m_backgroundRects.erase(it);
                m_backgroundRects.splice(m_backgroundRects.end(), std::move(result));
                goto retry;
            }
        }

        if (win->ShouldDrawDecoration()) {
            // Add a background clip under the titlebar
            m_backgroundRects.push_back({win->GetTitlebarRect(), true});
        }
    }
}
