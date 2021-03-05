#include <Lemon/IPC/Message.h>
#include <Lemon/IPC/Endpoint.h>

#include <string>

namespace Lemon::GUI{
    enum{
        WMCreateWindow = 100,
        WMDestroyWindow = 101,
        WMSetWindowTitle = 102,
        WMRelocateWindow = 103,
        WMResizeWindow = 104,
        WMMinimizeWindow = 105,
        WMMinimizeOtherWindow = 106,
        WMDisplayContextMenu = 107,
        WMInitializeShellConnection = 108,
        WMUpdateWindowFlags = 109,
        WMGetWindowPosition = 110,
    };

    enum {
        WindowBufferReturn = 100,
        WindowEvent = 101,
        WindowPositionReturn = 102,
    };

    class WMClient : public Endpoint {
    public:
        WMClient() : Endpoint("lemon.lemonwm/Instance"){

        }
        
        int64_t CreateWindow(int x, int y, int width, int height, unsigned int flags, const std::string& title) const;
        void DestroyWindow() const;

        void SetTitle(const std::string& title) const;
        void UpdateFlags(uint32_t flags) const;

        void Relocate(int x, int y) const;
        int64_t Resize(int width, int height) const;
        vector2i_t GetPosition() const;

        void Minimize(bool minimized) const;
        void Minimize(long windowID, bool minimized) const;
        void DisplayContextMenu(int x, int y, const std::string& serializedEntries) const;
        void InitializeShellConnection() const;
    };
}