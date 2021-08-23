#pragma once

#include "Compositor.h"
#include "Input.h"
#include "Window.h"

#include <Lemon/GUI/ContextMenu.h>
#include <Lemon/IPC/Interface.h>
#include <Lemon/Services/lemon.lemonwm.h>

#include <list>

#define CONTEXT_MENU_ITEM_WIDTH 100
#define CONTEXT_MENU_ITEM_HEIGHT 20

struct WMContextMenuEntry {
    int id;
    std::string text;
    Rect bounds;
};

class WM final : public LemonWMServer {
    friend int main();
    friend class Compositor;

public:
    inline static WM& Instance() {
        return *m_instance; // Assume main has initialize m_instance
    }

    void Run();

    inline Compositor& Compositor() { return m_compositor; }
    inline InputManager& Input() { return m_input; }

    void OnMouseDown(bool isRightButton);
    void OnMouseUp(bool isRightButton);
    void OnMouseMove();

    void OnKeyUpdate(int key, bool isPressed);

    inline const WMWindow* ActiveWindow() const { return m_activeWindow; };
    void SetActiveWindow(WMWindow* win);

    void BroadcastWindowState(WMWindow* win);

    inline void SetTargetFramerate(long fps) {
        m_targetFramerate = fps;
        if (fps > 0) {
            m_targetFrameInterval = 1000000 / fps;
            m_targetFrameIntervalThreshold = m_targetFrameInterval / 3 * 2;
        }
    }

private:
    static WM* m_instance;

    WM(const Surface& displaySurface);

    WMWindow* GetWindowFromID(int64_t id);

    inline int64_t NextWindowID() { return m_nextWindowID++; }

    void DestroyWindow(WMWindow* win);

    void BroadcastCreatedWindow(WMWindow* win);
    void BroadcastDestroyedWindow(WMWindow* win);

    void OnCreateWindow(const Lemon::Handle& client, int32_t x, int32_t y, int32_t width, int32_t height,
                        uint32_t flags, const std::string& title) override;
    void OnDestroyWindow(const Lemon::Handle& client, int64_t windowID) override;
    void OnSetTitle(const Lemon::Handle& client, int64_t windowID, const std::string& title) override;
    void OnUpdateFlags(const Lemon::Handle& client, int64_t windowID, uint32_t flags) override;
    void OnRelocate(const Lemon::Handle& client, int64_t windowID, int32_t x, int32_t y) override;
    void OnGetPosition(const Lemon::Handle& client, int64_t windowID) override;
    void OnResize(const Lemon::Handle& client, int64_t windowID, int32_t width, int32_t height) override;
    void OnMinimize(const Lemon::Handle& client, int64_t windowID, bool minimized) override;
    void OnDisplayContextMenu(const Lemon::Handle& client, int64_t windowID, int32_t x, int32_t y,
                              const std::string& entries) override;
    void OnPong(const Lemon::Handle& client, int64_t windowID) override;

    void OnPeerDisconnect(const Lemon::Handle& client) override;
    void OnGetScreenBounds(const Lemon::Handle& client) override;
    void OnReloadConfig(const Lemon::Handle& client) override;
    void OnSubscribeToWindowEvents(const Lemon::Handle& client) override;

    timespec m_lastUpdate;
    long m_targetFramerate = 0;              // Used for framerate limiter
    long m_targetFrameIntervalThreshold = 0; // The maximum time difference to thread sleep
    long m_targetFrameInterval = 0;

    Lemon::Interface m_messageInterface;
    class Compositor m_compositor;
    InputManager m_input;

    int m_resizePoint = ResizePoint::ResizePoint_None;

    bool m_draggingWindow = false;  // Dragging active window?
    Vector2i m_dragOffset = {0, 0}; // Offset of window being dragged

    int64_t m_nextWindowID = 1;
    // The active window is displayed on top and recieves keyboard input
    WMWindow* m_activeWindow = nullptr;
    // The last window the mouse was over
    WMWindow* m_lastMousedOver = nullptr;
    std::list<WMWindow*> m_windows;

    bool m_showContextMenu = false;
    struct {
        WMWindow* window = nullptr;
        std::vector<WMContextMenuEntry> entries;
        Rect bounds;
    } m_contextMenu;

    std::list<std::unique_ptr<LemonWMClientEndpoint>> m_wmEventSubscribers;
};