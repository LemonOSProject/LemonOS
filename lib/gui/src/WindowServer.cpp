#include <Lemon/GUI/WindowServer.h>

#include <Lemon/GUI/Theme.h>
#include <Lemon/GUI/Window.h>

#include <assert.h>

namespace Lemon {
WindowServer* WindowServer::m_instance = nullptr;
std::mutex WindowServer::m_instanceLock;

WindowServer* WindowServer::Instance() {
    if (m_instance) {
        return m_instance;
    }

    std::scoped_lock acquire(m_instanceLock);
    if (!m_instance) {
        m_instance = new WindowServer();
    }

    return m_instance;
}

void WindowServer::RegisterWindow(GUI::Window* win) { m_windows[win->ID()] = win; }

void WindowServer::UnregisterWindow(long windowID) { m_windows.erase(windowID); }

void WindowServer::SubscribeToWindowEvents() { LemonWMServerEndpoint::SubscribeToWindowEvents(); }

void WindowServer::Poll() {
    Lemon::Message m;
    while (Endpoint::Poll(m) > 0) {
        HandleMessage(m_handle, m);
    }
}

WindowServer::WindowServer() : LemonWMServerEndpoint("lemon.lemonwm/Instance") {
    assert(!m_instance);
    GUI::Theme::Current().Update(GetSystemTheme());
}

void WindowServer::OnPeerDisconnect(const Lemon::Handle&) {
    throw std::runtime_error("WindowServer has disconnected!");
}

void WindowServer::OnSendEvent(const Lemon::Handle&, int64_t windowID, int32_t id, uint64_t data) {
    if (auto window = m_windows.find(windowID);
        window != m_windows.end()) { // Event may have been sent before server acknowledged destroyed window
        window->second->m_eventQueue.push(Lemon::LemonEvent{.event = id, .data = data});
    }
}

void WindowServer::OnThemeUpdated(const Lemon::Handle&) { GUI::Theme::Current().Update(GetSystemTheme()); }

void WindowServer::OnPing(const Lemon::Handle&, int64_t windowID) { Pong(windowID); }

void WindowServer::OnWindowCreated(const Lemon::Handle&, int64_t windowID, uint32_t flags, const std::string& name) {
    if (OnWindowCreatedHandler)
        OnWindowCreatedHandler(windowID, flags, name);
}

void WindowServer::OnWindowStateChanged(const Lemon::Handle&, int64_t windowID, uint32_t flags, int32_t state) {
    if (OnWindowStateChangedHandler)
        OnWindowStateChangedHandler(windowID, flags, state);
}

void WindowServer::OnWindowTitleChanged(const Lemon::Handle&, int64_t windowID, const std::string& name) {
    if (OnWindowTitleChangedHandler)
        OnWindowTitleChangedHandler(windowID, name);
}

void WindowServer::OnWindowDestroyed(const Lemon::Handle&, int64_t windowID) {
    if (OnWindowDestroyedHandler)
        OnWindowDestroyedHandler(windowID);
}

} // namespace Lemon