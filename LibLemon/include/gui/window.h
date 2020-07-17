#pragma once

#include <core/msghandler.h>
#include <core/event.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/widgets.h>
#include <gui/ctxentry.h>
#include <utility>

#define WINDOW_FLAGS_NODECORATION 0x1
#define WINDOW_FLAGS_RESIZABLE 0x2
#define WINDOW_FLAGS_NOSHELL 0x4

#define WINDOW_MENUBAR_HEIGHT 20

namespace Lemon::GUI {
    __attribute__((unused)) static const char* wmSocketAddress = "lemonwm";

	typedef void(*WindowPaintHandler)(surface_t*);
    typedef void(*MessageReceiveHandler)();
    typedef void(*EventCallback)();

    enum {
        WMCreateWindow,
        WMDestroyWindow,
        WMSetTitle,
        WMRelocate,
        WMResize,
        WMUpdateFlags,
        WMMinimize,
        WMMinimizeOther,
        WMInitializeShellConnection,
        WMOpenContextMenu,
    };

    enum {
        WMCxtEntryTypeCommand,
        WMCxtEntryTypeDivider,
        WMCxtEntryTypeExpand,
    };

    struct WMCreateWindowCommand{
        vector2i_t size; // Window Size
        vector2i_t pos; // Window Position
        uint32_t flags; // Window Flags
        unsigned long bufferKey; // Shared Memory Key
        unsigned short titleLength; // Length (in bytes) of the title;
        char title[];
    };

    struct WMContextMenuEntry{
        unsigned short id;
        unsigned char length;
        char data[];
    };

    struct WMContextMenu{
        unsigned int contextEntryCount;
        WMContextMenuEntry contextEntries[];
    };

    struct WMSetTitleCommand{
        short cmd;
        unsigned short titleLength;
        char title[];
    } __attribute__((packed));

    struct WMCommand{
        short cmd;
        union{
            vector2i_t position;
            struct {
            vector2i_t size;
            unsigned long bufferKey;
            } __attribute__((packed));
            uint32_t flags;
            WMCreateWindowCommand create;
            struct{
            bool minimized;
            int minimizeWindowID;
            } __attribute__((packed));
            struct {
            unsigned short titleLength;
            char title[];
            } __attribute__((packed));
            struct {
                vector2i_t contextMenuPosition;
                WMContextMenu contextMenu;
            };
        };
    } __attribute__((packed));

    struct WindowBuffer {
        uint64_t currentBuffer;
        uint64_t buffer1Offset;
        uint64_t buffer2Offset;
        uint32_t drawing; // Is being drawn?
        uint32_t dirty; // Does it need to be drawn?
    };

    enum WindowType {
        Basic,
        GUI,
    };

    using WindowMenu = std::pair<std::string, std::vector<ContextMenuEntry>>;

    class WindowMenuBar : public Widget{
    public:
        std::vector<WindowMenu> items;

        void Paint(surface_t* surface);
        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
    };

    class Window {
    private:
        MessageClient msgClient;
        WindowBuffer* windowBufferInfo;
        uint8_t* buffer1;
        uint8_t* buffer2;
        uint64_t windowBufferKey;

        uint32_t flags;

        int windowType = WindowType::Basic;

        timespec lastClick;
    public:
        vector2i_t lastMousePos = {0, 0};
        WindowMenuBar* menuBar = nullptr;
        Container rootContainer;
        surface_t surface;
        bool closed = false; // Set to true when close button pressed

        Window(const char* title, vector2i_t size, uint32_t flags = 0, int type = WindowType::Basic, vector2i_t pos = {20, 20});
        ~Window();

        void SetTitle(const char* title);
        void Relocate(vector2i_t pos);
        void Resize(vector2i_t size);
        void Minimize(bool minimized = true);
        void Minimize(int windowID, bool minimized);

        void UpdateFlags(uint32_t flags);

        void Paint();
        void SwapBuffers();

        bool PollEvent(LemonEvent& ev);
        void WaitEvent();
        void GUIHandleEvent(LemonEvent& ev); // If the application decides to use the GUI they can pass events from PollEvent to here

        void AddWidget(Widget* w);
        void RemoveWidget(Widget* w);

        void DisplayContextMenu(std::vector<ContextMenuEntry>& entries, vector2i_t pos = {-1, -1});
        void CreateMenuBar();
        void (*OnMenuCmd)(unsigned short, Window*) = nullptr;

        uint32_t GetFlags() { return flags; }
        vector2i_t GetSize() { return {surface.width, surface.height}; };

        void SendWMMsg(LemonMessage* m) { msgClient.Send(m); }

        WindowPaintHandler OnPaint = nullptr;
    };
}