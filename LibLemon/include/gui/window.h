#pragma once

#include <core/msghandler.h>
#include <core/event.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/widgets.h>

#define WINDOW_FLAGS_NODECORATION 0x1
#define WINDOW_FLAGS_RESIZABLE 0x2

namespace Lemon::GUI {
    static const char* wmSocketAddress = "lemonwm";

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
    };

    struct WMCreateWindowCommand{
        vector2i_t size; // Window Size
        vector2i_t pos; // Window Position
        uint32_t flags; // Window Flags
        unsigned long bufferKey; // Shared Memory Key
        unsigned short titleLength; // Length (in bytes) of the title;
        char title[];
    };

    struct WMCommand{
        short cmd;
        unsigned short length;
        union{
            vector2i_t position;
            struct {
            vector2i_t size;
            unsigned long bufferKey;
            };
            uint32_t flags;
            WMCreateWindowCommand create;
            bool minimized;
            struct {
            unsigned short titleLength;
            char title[];
            };
        };
    };

    struct WindowBuffer {
        uint64_t currentBuffer;
        uint64_t buffer1Offset;
        uint64_t buffer2Offset;
        uint64_t padding; // Pad to 32 bytes
    };

    enum WindowType {
        Basic,
        GUI,
    };

    class Window {
    private:
        MessageClient msgClient;
        WindowBuffer* windowBufferInfo;
        uint8_t* buffer1;
        uint8_t* buffer2;
        uint64_t windowBufferKey;

        uint32_t flags;

        Container rootContainer;

        int windowType = WindowType::Basic;
    public:
        surface_t surface;
        bool closed = false; // Set to true when close button pressed

        Window(const char* title, vector2i_t size, uint32_t flags = 0, int type = WindowType::Basic, vector2i_t pos = {0, 0});
        ~Window();

        void SetTitle(const char* title);
        void Relocate(vector2i_t pos);
        void Resize(vector2i_t size);
        void Minimize(bool minimized = true);

        void UpdateFlags(uint32_t flags);

        void Paint();
        void SwapBuffers();

        bool PollEvent(LemonEvent& ev);
        void GUIHandleEvent(LemonEvent& ev); // If the application decides to use the GUI they can pass events from PollEvent to here

        void AddWidget(Widget* w);

        uint32_t GetFlags() { return flags; }
        vector2i_t GetSize() { return {surface.width, surface.height}; };

        WindowPaintHandler OnPaint = nullptr;
    };
}