#pragma once

#include <Lemon/GUI/Window.h>
#include <Lemon/Graphics/Types.h>

#include <string>

#define RESIZE_HANDLE_SIZE 8
#define MIN_WINDOW_RESIZE_WIDTH 50
#define MIN_WINDOW_RESIZE_HEIGHT 50

using namespace Lemon;

struct WindowTheme {
    int titlebarHeight = 24;
    int cornerRadius = 10;
    int borderWidth = 1;

    RGBAColour titlebarColour = {0x22, 0x20, 0x22, 0xFF};

    Surface windowButtons{};
};

enum ResizePoint {
    ResizePoint_None,
    ResizePoint_Top = 0x1,
    ResizePoint_Right = 0x2,
    ResizePoint_Bottom = 0x4,
    ResizePoint_Left = 0x8,
    ResizePoint_TopRight = ResizePoint_Top | ResizePoint_Right,
    ResizePoint_BottomRight = ResizePoint_Bottom | ResizePoint_Right,
    ResizePoint_BottomLeft = ResizePoint_Bottom | ResizePoint_Left,
    ResizePoint_TopLeft = ResizePoint_Top | ResizePoint_Left
};

class WMWindow : public LemonWMClientEndpoint {
public:
    WMWindow(const Handle& endpoint, int64_t id, const std::string& title, const Vector2i& pos, const Vector2i& size,
             int flags);

    void DrawDecorationClip(const Rect& clip, Surface* surface);
    void DrawClip(const Rect& clip, Surface* surface);

    inline int64_t GetID() const { return m_id; }
    inline int GetFlags() const { return m_flags; }
    inline const std::string& GetTitle() const { return m_title; }

    inline const Rect& GetRect() const { return m_rect; }
    inline const Vector2i& GetPosition() const { return m_rect.pos; }
    inline const Vector2i& GetSize() const { return m_rect.size; }

    // Exclude Window Decorations
    inline const Rect& GetContentRect() const { return m_contentRect; }
    inline const Vector2i& GetContentPosition() const { return m_contentRect.pos; }
    inline const Vector2i& GetContentSize() const { return m_contentRect.size; }

    inline const Rect& GetTitlebarRect() const { return m_titlebarRect; }
    // Get absolute close button rect
    inline const Rect& GetCloseRect() const { return m_closeRect; }

    // Top border resize handle
    inline const Rect& GetTopResizeRect() const { return m_borderRects[0]; }
    // Right border resize handle
    inline const Rect& GetRightResizeRect() const { return m_borderRects[1]; }
    // Bottom border resize handle
    inline const Rect& GetBottomResizeRect() const { return m_borderRects[2]; }
    // Left border resize handle
    inline const Rect& GetLeftResizeRect() const { return m_borderRects[3]; }
    // Check if the window has a resize handle at the given position
    int GetResizePoint(Vector2i absolutePosition) const;

    inline bool ShouldDrawDecoration() const { return !(m_flags & GUI::WindowFlag_NoDecoration); }
    inline bool IsResizable() const { return (m_flags & GUI::WindowFlag_Resizable); }
    inline bool IsTransparent() const { return (m_flags & GUI::WindowFlag_Transparent); }
    inline bool IsMinimized() const { return m_minimized; }
    inline bool HideWhenInactive() const { return (m_flags & GUI::WindowFlag_AlwaysActive); }
    inline bool NoShellEvents() const { return (m_flags & GUI::WindowFlag_NoShell); }

    // Get new window size from a rect
    // This will get the window content size accounting for the window decorations
    Vector2i NewWindowSizeFromRect(const Rect& rect) const;

    // Get whether the window buffer is dirty and regardless clear it
    inline bool IsDirtyAndClear() {
        bool isDirty = m_buffer->dirty;
        m_buffer->dirty = 0;
        return isDirty;
    }

    inline void SendEvent(const Lemon::LemonEvent& event) {
        LemonWMClientEndpoint::SendEvent(m_id, event.event, event.data);
    }

    void Relocate(int x, int y);
    inline void Relocate(const Vector2i& pos) { Relocate(pos.x, pos.y); };
    void Resize(int width, int height);

    void SetTitle(const std::string& title);
    void SetFlags(int flags);

    void Minimize(bool minimized = true);

    static WindowTheme theme;

private:
    void UpdateWindowRects();
    void CreateWindowBuffer();

    // Shared memory key for buffer
    int64_t m_bufferKey = 0;
    GUI::WindowBuffer* m_buffer;
    uint8_t* m_buffer1;
    uint8_t* m_buffer2;
    Surface m_windowSurface;

    int64_t m_id;

    std::string m_title;

    Vector2i m_size; // Window Content Size
    Rect m_rect;
    Rect m_contentRect;
    Rect m_titlebarRect;
    // Border rects for resizing
    // Top, Right, Bottom, Left
    Rect m_borderRects[4];
    Rect m_closeRect;

    int m_flags = 0;

    bool m_minimized = false;
};