#include <lemon/ipc/message.h>
#include <lemon/ipc/endpoint.h>

#include <string>

namespace Lemon::GUI{
    enum{
        WMCreateWindow = 100,
        WMDestroyWindow = 101,
        WMSetWindowTitle = 102,
        WMRelocateWindow = 103,
        WMResizeWindow = 104,
        WMMinimizeWindow = 105,
        WMMinimizeOtherWindow = 105,
        WMDisplayContextMenu = 106,
        WMInitializeShellConnection = 107,
    };

    class WMClient : public Endpoint {
    public:
        WMClient() : Endpoint("lemon.lemonwm/Instance"){

        }
        
        int64_t CreateWindow(int x, int y, int width, int height, unsigned int flags, const std::string& title) const;
        void DestroyWindow() const;

        void SetTitle(const std::string& title) const;
        void Relocate(int x, int y) const;
        uint64_t Resize(int width, int height) const;
        void Minimize(bool minimized) const;
        void Minimize(long windowID, bool minimized) const;
        void DisplayContextMenu(int x, int y, const std::string& serializedEntries) const;
        void InitializeShellConnection() const;
    };
}