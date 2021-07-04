#pragma once

#include <Lemon/Services/lemon.lemonwm.h>

#include <Lemon/Graphics/Types.h>

#include <mutex>
#include <map>

namespace Lemon {
    namespace GUI { class Window; }

    class WindowServer final
        : public LemonWMServerEndpoint, LemonWMClient {
    public:
        static WindowServer* Instance();

        void RegisterWindow(GUI::Window* win);
        void UnregisterWindow(long windowID);

        void Poll();

        /////////////////////////////
        /// \brief Get Screen Bounds
        ///
        /// \return screen width and height
        /////////////////////////////
        inline vector2i_t GetScreenBounds() {
            LemonWMServer::GetScreenBoundsResponse r = LemonWMServerEndpoint::GetScreenBounds();

            return { r.width, r.height };
        }
    private:
        WindowServer();

        void OnPeerDisconnect(handle_t client) override;
        void OnSendEvent(handle_t client, int64_t windowID, int32_t id, uint64_t data) override;
        void OnThemeUpdate(handle_t client, const std::string& name) override;
        void OnPing(handle_t client, int64_t windowID) override;

        std::map<long, GUI::Window*> m_windows;

        static WindowServer* m_instance;
        static std::mutex m_instanceLock;
    };
}