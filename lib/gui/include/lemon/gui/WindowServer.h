#pragma once

#include <lemon/protocols/lemon.lemonwm.h>

#include <lemon/graphics/Types.h>

#include <map>
#include <mutex>

namespace Lemon {
namespace GUI {
class Window;
}

enum WindowState {
    WindowState_Normal = 0,
    WindowState_Minimized = 1,
    WindowState_Active = 2,
};

class WindowServer final : public LemonWMServerEndpoint, LemonWMClient {
public:
    static WindowServer* Instance();

    void RegisterWindow(GUI::Window* win);
    void UnregisterWindow(long windowID);

    void SubscribeToWindowEvents();

    void Poll();

    /////////////////////////////
    /// \brief Get Screen Bounds
    ///
    /// \return screen width and height
    /////////////////////////////
    inline vector2i_t GetScreenBounds() {
        LemonWMServer::GetScreenBoundsResponse r = LemonWMServerEndpoint::GetScreenBounds();

        return {r.width, r.height};
    }

    /////////////////////////////
    /// \brief Get path to system theme
    ///
    /// \return Path to system theme
    /////////////////////////////
    inline std::string GetSystemTheme() {
        LemonWMServer::GetSystemThemeResponse r = LemonWMServerEndpoint::GetSystemTheme();

        return r.path;
    }

    void (*OnWindowCreatedHandler)(int64_t, uint32_t, const std::string&) = nullptr;
    void (*OnWindowStateChangedHandler)(int64_t, uint32_t, int32_t) = nullptr;
    void (*OnWindowTitleChangedHandler)(int64_t, const std::string&) = nullptr;
    void (*OnWindowDestroyedHandler)(int64_t) = nullptr;

private:
    WindowServer();

    void OnPeerDisconnect(const Lemon::Handle& client) override;
    void OnSendEvent(const Lemon::Handle& client, int64_t windowID, int32_t id, uint64_t data) override;
    void OnThemeUpdated(const Lemon::Handle& client) override;
    void OnPing(const Lemon::Handle& client, int64_t windowID) override;

    void OnWindowCreated(const Lemon::Handle& client, int64_t windowID, uint32_t flags, const std::string& name) override;
    void OnWindowStateChanged(const Lemon::Handle& client, int64_t windowID, uint32_t flags, int32_t state) override;
    void OnWindowTitleChanged(const Lemon::Handle& client, int64_t windowID, const std::string& name) override;
    void OnWindowDestroyed(const Lemon::Handle& client, int64_t windowID) override;

    std::map<long, GUI::Window*> m_windows;

    static WindowServer* m_instance;
    static std::mutex m_instanceLock;
};
} // namespace Lemon