#include "Window.h"

#include "WM.h"

#include <Lemon/Core/SharedMemory.h>

WindowTheme WMWindow::theme;

WMWindow::WMWindow(const Handle& endpoint, int64_t id, const std::string& title, const Vector2i& pos,
                   const Vector2i& size, int flags)
    : LemonWMClientEndpoint(endpoint), m_id(id), m_title(title), m_size(size), m_rect(Rect{pos, size}), m_flags(flags) {
    CreateWindowBuffer();
    Queue(Lemon::Message(LemonWMServer::ResponseCreateWindow, LemonWMServer::CreateWindowResponse{m_id, m_bufferKey}));

    UpdateWindowRects();

    WM::Instance().Compositor().InvalidateAll();
}

void WMWindow::DrawDecorationClip(const Rect& clip, Surface* surface) {
    if (!ShouldDrawDecoration()) {
        return;
    }

    if (clip.Intersects(m_titlebarRect)) {
        Graphics::DrawRoundedRect(m_titlebarRect, theme.titlebarColour, theme.cornerRadius, theme.cornerRadius, 0, 0,
                                  surface, clip);

        Graphics::DrawString(m_title.c_str(), m_titlebarRect.pos.x + theme.cornerRadius + 2,
                             m_titlebarRect.pos.y + (m_titlebarRect.size.y - Graphics::DefaultFont()->pixelHeight) / 2,
                             {0xff, 0xff, 0xff, 0xff}, surface, clip);
    }
    Graphics::DrawRectOutline(
        {Vector2i{m_rect.x, m_titlebarRect.bottom()}, Vector2i{m_rect.width, m_contentRect.height + theme.borderWidth}},
        theme.titlebarColour, surface, clip);

    Rect closeButtonSourceRect = {0, 0, theme.windowButtons.width / 2, theme.windowButtons.height / 2};
    if (m_closeRect.Contains(WM::Instance().Input().mouse.pos)) {
        // The mouse hover state window buttons are on next row of image file
        closeButtonSourceRect.y += theme.windowButtons.height / 2;
    }

    if (clip.Contains(m_closeRect)) {
        surface->AlphaBlit(&theme.windowButtons, m_closeRect.pos, closeButtonSourceRect);
    }
}

void WMWindow::DrawClip(const Rect& clip, Surface* surface) {
    m_windowSurface.buffer = m_buffer->currentBuffer ? (m_buffer2) : (m_buffer1);

    Rect clipCopy = clip;
    clipCopy.pos -= m_contentRect.pos;

    if(IsTransparent()){
        surface->AlphaBlit(&m_windowSurface, clip.pos, clipCopy);
    } else {
        surface->Blit(&m_windowSurface, clip.pos, clipCopy);
    }
}

int WMWindow::GetResizePoint(Vector2i absolutePosition) const {
    int point = ResizePoint_None;

    if (m_borderRects[0].Contains(absolutePosition)) {
        point |= ResizePoint_Top;
    } else if (m_borderRects[2].Contains(absolutePosition)) {
        point |= ResizePoint_Bottom;
    }

    if (m_borderRects[1].Contains(absolutePosition)) {
        point |= ResizePoint_Right;
    } else if (m_borderRects[3].Contains(absolutePosition)) {
        point |= ResizePoint_Left;
    }

    return point;
}

Vector2i WMWindow::NewWindowSizeFromRect(const Rect& rect) const {
    Vector2i newSize = rect.size;
    if(ShouldDrawDecoration()){
        newSize.x = std::max(newSize.x - theme.borderWidth * 2, MIN_WINDOW_RESIZE_WIDTH);
        newSize.y = std::max(newSize.y - theme.borderWidth * 2 - theme.titlebarHeight, MIN_WINDOW_RESIZE_HEIGHT);
    }

    return newSize;
}

void WMWindow::Relocate(int x, int y) {
    m_rect.pos = {x, y};

    UpdateWindowRects();

    WM::Instance().Compositor().InvalidateAll(); // Window position changed, recaclulate clipping
}

void WMWindow::Resize(int width, int height) {
    long e = Lemon::DestroySharedMemory(m_bufferKey);
    assert(!e);

    m_size = {width, height};

    CreateWindowBuffer();
    Queue(Lemon::Message(LemonWMServer::ResponseResize, LemonWMServer::ResizeResponse{m_bufferKey}));
    UpdateWindowRects();

    WM::Instance().Compositor().InvalidateAll(); // Window size changed, recaclulate clipping
}

void WMWindow::SetTitle(const std::string& title) {
    m_title = title;

    WM::Instance().Compositor().InvalidateWindow(this); // Redraw window decorations
}
void WMWindow::SetFlags(int flags) {
    int oldFlags = m_flags;

    m_flags = flags;

    const int invalidatingFlags = GUI::WindowFlag_NoDecoration | GUI::WindowFlag_Transparent;
    if ((oldFlags & invalidatingFlags) != (m_flags & invalidatingFlags)) {
        UpdateWindowRects();

        WM::Instance().Compositor().InvalidateAll(); // We will need to recalculate clipping
    }
}

void WMWindow::Minimize(bool minimized) {
    if (minimized == m_minimized) {
        return; // Nothing to do
    }

    if (minimized) {
        if (WM::Instance().ActiveWindow() == this) {
            WM::Instance().SetActiveWindow(nullptr);
        }

        SendEvent({.event = EventWindowMinimized});
    } else {
        WM::Instance().SetActiveWindow(this); // When the window is being shown again set it as active
    }

    m_minimized = minimized;

    WM::Instance().Compositor().InvalidateAll(); // Window state changed, recaclulate clipping
    WM::Instance().BroadcastWindowState(this);
}

void WMWindow::UpdateWindowRects() {
    if (ShouldDrawDecoration()) {
        m_contentRect.y = m_rect.top() + theme.titlebarHeight + theme.borderWidth;
        m_contentRect.x = m_rect.x + theme.borderWidth;
        m_contentRect.size = m_size;

        m_rect.size = {m_size.x + theme.borderWidth * 2, m_size.y + theme.titlebarHeight + theme.borderWidth * 2};

        m_titlebarRect = {m_rect.x, m_rect.y, m_rect.size.x, theme.titlebarHeight + theme.borderWidth};

        m_borderRects[0] = {m_rect.pos, m_rect.size.x, RESIZE_HANDLE_SIZE}; // Top
        m_borderRects[1] = {m_rect.pos.x + m_rect.size.x - RESIZE_HANDLE_SIZE, m_rect.pos.y, RESIZE_HANDLE_SIZE,
                            m_rect.size.y}; // Right
        m_borderRects[2] = {m_rect.pos.x, m_rect.pos.y + m_rect.size.y - RESIZE_HANDLE_SIZE, m_rect.size.x,
                            RESIZE_HANDLE_SIZE};
        m_borderRects[3] = {m_rect.pos, RESIZE_HANDLE_SIZE, m_rect.size.y}; // Left

        m_closeRect = {m_rect.x + m_rect.width - 2 - theme.windowButtons.width / 2,
                       m_rect.y + theme.titlebarHeight / 2 - theme.windowButtons.height / 4,
                       theme.windowButtons.width / 2, theme.windowButtons.height / 2};
    } else {
        m_contentRect = m_rect;
    }
}

void WMWindow::CreateWindowBuffer() {
    // Size of each buffer for the window
    // Aligned to 32 bytes
    unsigned bufferSize = (m_size.x * m_size.y * 4 + 0x1F) & (~0x1FULL);
    if(m_bufferKey){
        Lemon::UnmapSharedMemory(m_buffer, m_bufferKey);
    }

    m_bufferKey = Lemon::CreateSharedMemory(((sizeof(GUI::WindowBuffer) + 0x1F) & (~0x1FULL)) + bufferSize * 2,
                                            SMEM_FLAGS_SHARED);
    m_buffer = reinterpret_cast<GUI::WindowBuffer*>(Lemon::MapSharedMemory(m_bufferKey));

    memset(m_buffer, 0, sizeof(GUI::WindowBuffer));
    m_buffer->buffer1Offset = ((sizeof(GUI::WindowBuffer) + 0x1F) & (~0x1FULL));
    m_buffer->buffer2Offset = ((sizeof(GUI::WindowBuffer) + 0x1F) & (~0x1FULL)) + bufferSize;

    m_buffer1 = reinterpret_cast<uint8_t*>(m_buffer) + m_buffer->buffer1Offset;
    m_buffer2 = reinterpret_cast<uint8_t*>(m_buffer) + m_buffer->buffer2Offset;

    m_windowSurface = {.width = m_size.x, .height = m_size.y, .buffer = m_buffer1};

    assert(!((m_buffer->buffer1Offset | m_buffer->buffer2Offset) & 0x1F)); // Ensure alignment
}