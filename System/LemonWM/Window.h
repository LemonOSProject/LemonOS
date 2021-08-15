#pragma once

#include <Lemon/Graphics/Types.h>
#include <Lemon/GUI/Window.h>

#include <string>

using namespace Lemon;

struct WindowTheme {
    int titlebarHeight = 20;
    int cornerRadius = 5;
    int borderWidth = 1;
};

class WMWindow 
    : public LemonWMClientEndpoint {
public:
    WMWindow(const Handle& endpoint, int64_t id, const std::string& title, const Vector2i& pos, const Vector2i& size, int flags);

    inline int64_t GetID() const { return m_id; }

    inline const Rect& GetRect() const { return m_rect; }
    inline const Vector2i& GetPosition() const { return m_rect.pos; }
    inline const Vector2i& GetSize() const { return m_rect.size; }

    // Exclude Window Decorations
    inline const Rect& GetContentRect() const { return m_contentRect; }
    inline const Vector2i& GetContentPosition() const { return m_contentRect.pos; }
    inline const Vector2i& GetContentSize() const { return m_contentRect.size; }

    inline bool ShouldDrawDecoration() const { return !(m_flags & GUI::WindowFlag_NoDecoration); }
    inline bool IsResizable() const { return (m_flags & GUI::WindowFlag_Resizable); }
    inline bool IsTransparent() const { return (m_flags & GUI::WindowFlag_Transparent); }
    inline bool IsMinimized() const { return m_minimized; }

    void Relocate(int x, int y);
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

    int64_t m_id;

    std::string m_title;

    Vector2i m_size; // Window Content Size
    Rect m_rect;
    Rect m_contentRect; 
    int m_flags = 0;

    bool m_minimized = false;
};