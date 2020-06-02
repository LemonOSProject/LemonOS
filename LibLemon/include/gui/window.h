#pragma once

#include <core/msghandler.h>
#include <core/event.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>

#define WINDOW_FLAGS_NODECORATION 0x1

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
            vector2i_t size;
            uint32_t flags;
            WMCreateWindowCommand create;
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

    class Window {
    private:
        MessageClient msgClient;
        WindowBuffer* windowBufferInfo;
        uint8_t* buffer1;
        uint8_t* buffer2;
        uint64_t windowBufferKey;

        uint32_t flags;
    public:
        surface_t surface;

        Window(const char* title, vector2i_t size, vector2i_t pos = {0, 0}, uint32_t flags = 0);
        ~Window();

        void SetTitle(const char* title);
        void Relocate(vector2i_t pos);
        void Resize(vector2i_t size);

        void UpdateFlags(uint32_t flags);

        uint32_t GetFlags() { return flags; }
        vector2i_t GetSize() { return {surface.width, surface.height}; };

        void Paint();
        void SwapBuffers();
        bool PollEvent(LemonEvent& ev);

        WindowPaintHandler OnPaint = nullptr;
        MessageReceiveHandler OnReceiveMessage = nullptr;
    };
}