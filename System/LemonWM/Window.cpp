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
    if((oldFlags & invalidatingFlags) != (m_flags & invalidatingFlags)) {
        UpdateWindowRects();

        WM::Instance().Compositor().InvalidateAll(); // We will need to recalculate clipping
    }
}

void WMWindow::Minimize(bool minimized) {
    if(minimized == m_minimized) {
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
    } else {
        m_contentRect = m_rect;
    }
}

void WMWindow::CreateWindowBuffer() {
    m_bufferKey = Lemon::CreateSharedMemory(sizeof(GUI::WindowBuffer) + m_size.x * m_size.y * 4 * 2, SMEM_FLAGS_SHARED);
    m_buffer = reinterpret_cast<GUI::WindowBuffer*>(Lemon::MapSharedMemory(m_bufferKey));
}