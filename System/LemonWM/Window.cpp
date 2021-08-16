#include "Window.h"

#include "WM.h"

#include <Lemon/Core/SharedMemory.h>

WindowTheme WMWindow::theme;

WMWindow::WMWindow(const Handle& endpoint, int64_t id, const std::string& title, const Vector2i& pos,
                   const Vector2i& size, int flags)
    : LemonWMClientEndpoint(endpoint), m_id(id), m_title(title), m_size(size), m_rect(Rect{pos, size}), m_flags(flags) {
    CreateWindowBuffer();

    UpdateWindowRects();

    WM::Instance().Compositor().InvalidateAll();
}

void WMWindow::DrawClip(const Rect& clip, Surface* surface) {
    if (ShouldDrawDecoration()) {
        if (clip.Intersects(m_titlebarRect)) {
            Graphics::DrawRoundedRect(m_titlebarRect, theme.titlebarColour, theme.cornerRadius, theme.cornerRadius, 0,
                                      0, surface, clip);
        
            Graphics::DrawString(m_title.c_str(), m_titlebarRect.pos.x + theme.cornerRadius + 2, m_titlebarRect.pos.y + (m_titlebarRect.size.y - Graphics::DefaultFont()->pixelHeight) / 2, {0xff, 0xff, 0xff, 0xff}, surface, clip);
        }
        Graphics::DrawRectOutline({Vector2i{m_rect.x, m_titlebarRect.bottom()}, m_contentRect.size}, theme.titlebarColour, surface, clip);
    }
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

    m_minimized = minimized;

    WM::Instance().Compositor().InvalidateAll(); // Window state changed, recaclulate clipping
}

void WMWindow::UpdateWindowRects() {
    if (ShouldDrawDecoration()) {
        m_contentRect.top(m_rect.top() + theme.titlebarHeight + theme.borderWidth);
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
    } else {
        m_contentRect = m_rect;
    }
}

void WMWindow::CreateWindowBuffer() {
    // Size of each buffer for the window
    // Aligned to 32 bytes
    unsigned bufferSize = (m_size.x * m_size.y * 4 + 0x1F) & (~0x1FULL);

    m_bufferKey = Lemon::CreateSharedMemory(((sizeof(GUI::WindowBuffer) + 0x1F) & (~0x1FULL)) + bufferSize * 2,
                                            SMEM_FLAGS_SHARED);
    m_buffer = reinterpret_cast<GUI::WindowBuffer*>(Lemon::MapSharedMemory(m_bufferKey));

    memset(m_buffer, 0, sizeof(GUI::WindowBuffer));
    m_buffer->buffer1Offset = ((sizeof(GUI::WindowBuffer) + 0x1F) & (~0x1FULL));
    m_buffer->buffer2Offset = ((sizeof(GUI::WindowBuffer) + 0x1F) & (~0x1FULL)) + bufferSize;

    assert(!((m_buffer->buffer1Offset | m_buffer->buffer2Offset) & 0x1F)); // Ensure alignment
}