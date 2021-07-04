#include <Lemon/GUI/WindowServer.h>

#include <Lemon/GUI/Window.h>

#include <assert.h>

namespace Lemon {
    WindowServer* WindowServer::m_instance = nullptr;
    std::mutex WindowServer::m_instanceLock;

    WindowServer* WindowServer::Instance(){
        if(m_instance){
            return m_instance;
        }

        std::scoped_lock acquire(m_instanceLock);
        if(!m_instance){
            m_instance = new WindowServer();
        }

        return m_instance;
    }

    void WindowServer::RegisterWindow(GUI::Window* win){
        m_windows[win->ID()] = win;
    }

    void WindowServer::UnregisterWindow(long windowID){
        m_windows.erase(windowID);
    }

    void WindowServer::Poll(){
        Lemon::Message m;
        while(Endpoint::Poll(m) > 0){
            HandleMessage(handle, m);
        }
    }

    WindowServer::WindowServer()
        : LemonWMServerEndpoint("lemon.lemonwm/Instance"){
        assert(!m_instance);
    }

    void WindowServer::OnPeerDisconnect(handle_t) {
        throw std::runtime_error("WindowServer has disconnected!");
    }

    void WindowServer::OnSendEvent(handle_t, int64_t windowID, int32_t id, uint64_t data) {
        m_windows.at(windowID)->eventQueue.push(Lemon::LemonEvent{ .event = id, .data = data });
    }

    void WindowServer::OnThemeUpdate(handle_t, const std::string& name) {
        (void)name;
    }

    void WindowServer::OnPing(handle_t, int64_t windowID) {
        Pong(windowID);
    }
}