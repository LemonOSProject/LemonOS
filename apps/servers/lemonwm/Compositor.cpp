#include "Compositor.h"

#include "WM.h"

#include <lemon/core/Logger.h>
#include <lemon/graphics/Graphics.h>

//#define COMPOSITOR_DEBUG

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

    clock_gettime(CLOCK_BOOTTIME, &m_lastRender);
}

void Compositor::Render() {
    if (m_displayFramerate) {
        timespec cTime;
        clock_gettime(CLOCK_BOOTTIME, &cTime);

        unsigned long renderTime =
            (cTime.tv_nsec - m_lastRender.tv_nsec) + (cTime.tv_sec - m_lastRender.tv_sec) * 1000000000L;
        m_fCount++;

        if (renderTime > 1000000000 && m_fCount) {

            if (renderTime)
                m_fRate = 1000000000 / (renderTime / m_fCount);
            m_fCount = 0;
            renderTime = 0;
            m_lastRender = cTime;
        }
    }

    Vector2i mousePos = WM::Instance().Input().mouse.pos;
    if (mousePos != m_lastMousePos) {
        Invalidate({m_lastMousePos, m_cursorCurrent->width, m_cursorCurrent->height});
        m_lastMousePos = mousePos;
    }

    m_renderMutex.lock();

    if (m_wallpaperThread.joinable() && m_wallpaperStatus) {
        m_wallpaperThread.join();
    }

    if (m_invalidateAll) {
        RecalculateBackgroundClipping();
        RecalculateWindowClipping();
        RecalculateRenderClipping();

        // We fill the areas of the screen being redrawn in debug mode
        // This happens before the render surface is blitted to the display surface
#ifdef COMPOSITOR_DEBUG
        Lemon::Graphics::DrawRect(0, 0, m_displaySurface.width, m_displaySurface.height, {255, 0, 0, 255},
                                  &m_displaySurface);
#endif

        if (m_wallpaper.buffer) {
            m_renderSurface.Blit(&m_wallpaper);
            for (BackgroundClipRect& rect : m_backgroundRects) {
#ifdef COMPOSITOR_DEBUG
                Lemon::Graphics::DrawRectOutline(rect.rect, {255, 255, 0, 255}, &m_renderSurface);
#endif
                rect.invalid = false;
            }
        }
    } else {
        for (WMWindow* win : WM::Instance().m_windows) {
            if (win->IsDirtyAndClear()) {
                // If the window is transparent, we will need to invalidate
                // any rects underneath the window

                // If the window is occluded, we will need to invalidate any rects
                // above the clip

                // Otherwise, just set the window rect as invalid
                // and move on

                if(win->IsTransparent()) for(auto& r : m_windowClipRects) {
                    if(r.win == win) {
                        Invalidate(r.rect);
                    }
                } else for(auto& r : m_windowClipRects) {
                    if(r.win == win) {
                        if(r.occluded) {
                            Invalidate(r.rect);
                        } else {
                            r.invalid = true;
                        }
                    }
                }
                
            }
        }
    }

    if (m_wallpaper.buffer) {
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

        if (m_invalidateAll || it->invalid) { // Window buffer not dirty, only draw invalid clips
            if (it->type == WindowClipRect::TypeWindowDecoration) {
                win->DrawDecorationClip(it->rect, &m_renderSurface);
            } else {
                win->DrawClip(it->rect, &m_renderSurface);
            }

#ifdef COMPOSITOR_DEBUG
            Lemon::Graphics::DrawRect(it->rect, {255, 0, 0, 255}, &m_displaySurface);
            if(it->type == WindowClipRect::TypeWindowDecoration)
                Lemon::Graphics::DrawRectOutline(it->rect, {128, 128, 255, 255}, &m_renderSurface);
            else
                Lemon::Graphics::DrawRectOutline(it->rect, {0, 0, 255, 255}, &m_renderSurface);
#endif

            it->invalid = false;
            it++;
        } else {
            it++;
        }
    }

    if (WM::Instance().m_showContextMenu) {
        Lemon::Graphics::DrawRoundedRect(WM::Instance().m_contextMenu.bounds, WMWindow::theme.titlebarColour, 5, 5, 5,
                                         5, &m_renderSurface);
        for (const auto& ent : WM::Instance().m_contextMenu.entries) {
            const Vector2i& pos = ent.bounds.pos;
            Lemon::Graphics::DrawString(ent.text.c_str(), pos.x, pos.y, GUI::Theme::Current().ColourText(),
                                        &m_renderSurface);
        }
    }

    m_renderSurface.AlphaBlit(m_cursorCurrent, mousePos);

    if (m_displayFramerate) {
        Lemon::Graphics::DrawRect(0, 0, 80, 18, 0, 0, 0, &m_renderSurface);
        Lemon::Graphics::DrawString(std::to_string(m_fRate).c_str(), 0, 0, 255, 255, 255, &m_renderSurface);
    }

    // Copy the render surface to the display surface
    // Generally actually faster than copying each individual clip region
    m_displaySurface.Blit(&m_renderSurface);
    m_invalidateAll = false;

    m_renderMutex.unlock();
}

void Compositor::InvalidateAll() { m_invalidateAll = true; }

void Compositor::Invalidate(const Rect& rect) {
    if (m_invalidateAll) {
        return;
    }

    for (auto& bgRect : m_backgroundRects) {
        if (bgRect.invalid) {
            continue;
        }

        if (bgRect.rect.Intersects(rect)) {
            bgRect.invalid = true; // Set bg rect as invalid
        }

        for (auto& wRect : m_windowClipRects) {
            if (wRect.rect.Intersects(bgRect.rect)) {
                wRect.invalid = true;
            }
        }
    }

    for (auto& wRect : m_windowClipRects) {
        if (wRect.invalid) {
            for (auto* bgRect : wRect.win->occludedBackgroundRects) {
                bgRect->invalid = true;
            }

            for (auto* oRect : wRect.win->occludedWindowRects) {
                oRect->invalid = true;
            }
            continue;
        }

        if (wRect.rect.Intersects(rect)) {
            wRect.invalid = true;
            for (auto* bgRect : wRect.win->occludedBackgroundRects) {
                bgRect->invalid = true;
            }

            for (auto* oRect : wRect.win->occludedWindowRects) {
                oRect->invalid = true;
            }
        }
    }

    for(auto& rRect : m_renderClipRects) {
        if(rRect.rect.Intersects(rect)) {
            rRect.invalid = true;
        }
    }
}

void Compositor::InvalidateWindow(class WMWindow* window) { Invalidate(window->GetContentRect()); }

void Compositor::SetWallpaper(const std::string& path) {
    m_wallpaperStatus = 0;
    std::thread wallpaperThread([this, path]() -> void {
        std::unique_lock lock(m_wallpaperMutex);

        if (Graphics::LoadImage(path.c_str(), 0, 0, m_displaySurface.width, m_displaySurface.height, &m_wallpaper,
                                true)) {
            Logger::Error("Failed to load wallpaper '{}'", path);
        }

        m_renderMutex.lock();
        InvalidateAll();
        m_wallpaperStatus++;
        m_renderMutex.unlock();
    });

    m_wallpaperThread = std::move(wallpaperThread);
}

void Compositor::RecalculateWindowClipping() {
    m_windowClipRects.clear();

    for (WMWindow* win : WM::Instance().m_windows) {
        win->occludedBackgroundRects.clear();
        win->occludedWindowRects.clear();

        if (win->IsMinimized()) {
            continue;
        }

    retry:
        for (auto it = m_windowClipRects.begin(); it != m_windowClipRects.end(); it++) {
            if (win->IsTransparent() && win->GetContentRect().Contains(it->rect)) {
                continue;
            }

            if (it->rect.Intersects(win->GetContentRect())) {
                auto result = it->SplitModify(win->GetContentRect());

                if (!win->IsTransparent()) {
                    m_windowClipRects.erase(it);
                }

                m_windowClipRects.splice(m_windowClipRects.end(), std::move(result));
                goto retry;
            }
        }

        if (win->ShouldDrawDecoration()) {
            auto decorationRects =
                WindowClipRect{win->GetRect(), win, WindowClipRect::TypeWindowDecoration, true}.Split(win->GetContentRect());

            for (auto& rect : decorationRects) {
                for (auto it = m_windowClipRects.begin(); it != m_windowClipRects.end(); it++) {
                    // Decoration rects are treated as transparent
                    if (rect.rect.Contains(it->rect)) {
                        rect.occluded = true;
                        continue;
                    }

                    if (it->rect.Intersects(rect.rect)) {
                        auto result = it->SplitModify(rect.rect);
                        // Instead of removing the clip, mark it as occluded
                        it->occluded = true;

                        m_windowClipRects.splice(m_windowClipRects.end(), std::move(result));
                        goto retry;
                    }
                }
            }

            m_windowClipRects.splice(m_windowClipRects.end(), std::move(decorationRects));
        }

        m_windowClipRects.push_back({win->GetContentRect(), win, WindowClipRect::TypeWindow});

        if (win->IsTransparent()) {
            for (auto& bgRect : m_backgroundRects) {
                if (bgRect.rect.Intersects(win->GetContentRect())) {
                    win->occludedBackgroundRects.push_back(&bgRect);
                }
            }

            for (auto& bgRect : m_windowClipRects) {
                if (bgRect.rect.Intersects(win->GetContentRect())) {
                    win->occludedWindowRects.push_back(&bgRect);
                }
            }
        }
    }
}

void Compositor::RecalculateBackgroundClipping() {
    m_backgroundRects.clear();

    m_backgroundRects.push_back({Rect{0, 0, m_displaySurface.width, m_displaySurface.height}, true});

    auto& windows = WM::Instance().m_windows;
    for (WMWindow* win : windows) {
        if (win->IsMinimized()) {
            continue;
        }

        std::list<Rect> splitDecorationRects;
        if (win->ShouldDrawDecoration()) {
            splitDecorationRects = win->GetRect().Split(win->GetContentRect());
        }

    retry:
        for (auto it = m_backgroundRects.begin(); it != m_backgroundRects.end(); it++) {
            if (win->ShouldDrawDecoration()) {
                for (auto& dRect : splitDecorationRects) {
                    if (it->rect.Intersects(dRect) && !dRect.Contains(it->rect)) {
                        auto result = it->SplitModify(dRect);

                        m_backgroundRects.splice(m_backgroundRects.end(), std::move(result));
                        goto retry;
                    }
                }
            }

            if (win->IsTransparent() && win->GetContentRect().Contains(it->rect)) {
                continue; // Background rect entirely within transparent window
            }

            if (it->rect.Intersects(win->GetContentRect())) {
                auto result = it->SplitModify(win->GetContentRect());

                if (!win->IsTransparent()) {
                    m_backgroundRects.erase(it);
                }
                m_backgroundRects.splice(m_backgroundRects.end(), std::move(result));
                goto retry;
            }
        }
    }
}

void Compositor::RecalculateRenderClipping() {
    m_renderClipRects.clear();
    for(auto& rect : m_backgroundRects) {
        m_renderClipRects.push_back({rect.rect, true});
    }

    for(auto& rect : m_windowClipRects) {
        // Ignore transparent clip rects
        if(rect.type == WindowClipRect::TypeWindow && !rect.win->IsTransparent()) {
            m_renderClipRects.push_back({rect.rect, true});
        }
    }
}
