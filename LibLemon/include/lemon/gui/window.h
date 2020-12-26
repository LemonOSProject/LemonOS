#pragma once

#include <lemon/core/event.h>
#include <lemon/gfx/surface.h>
#include <lemon/gfx/graphics.h>
#include <lemon/gui/widgets.h>
#include <lemon/gui/ctxentry.h>
#include <lemon/gui/wmclient.h>
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
        WindowBufferReturn = 100,
        WindowEvent = 101,
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
    };

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

    class Window : protected WMClient{
    private:
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
        surface_t surface = {0, 0, 32, nullptr};
        bool closed = false; // Set to true when close button pressed

        Window(const char* title, vector2i_t size, uint32_t flags = 0, int type = WindowType::Basic, vector2i_t pos = {20, 20});
        ~Window();

        /////////////////////////////
        /// \brief Set title of window
        ///
        /// \param name Service name to be used, must be unique
        /////////////////////////////
        inline void SetTitle(const std::string& title) const { WMClient::SetTitle(title); }
        
        /////////////////////////////
        /// \brief Relocate window
        ///
        /// Move the window to position (x, y) on screen
        ///
        /// \param pos New position of window
        /////////////////////////////
        inline void Relocate(vector2i_t pos) const { WMClient::Relocate(pos.x, pos.y); }

        /////////////////////////////
        /// \brief Resize window
        ///
        /// Resize the window
        ///
        /// \param size New size of window
        /////////////////////////////
        void Resize(vector2i_t size);
        
        /////////////////////////////
        /// \brief Minimize window
        ///
        /// Show/hide the window on screen 
        ///
        /// \param minimized Whether to show/hide the window
        /////////////////////////////
        inline void Minimize(bool minimized = true) const { WMClient::Minimize(minimized); }

        /////////////////////////////
        /// \brief Minimize a window
        ///
        /// Show/hide the specified window on screen 
        ///
        /// \param windowID Window to show/hide
        /// \param minimized Whether to show/hide the window
        /////////////////////////////
        inline void Minimize(int windowID, bool minimized) const { WMClient::Minimize(windowID, minimized); }

        /////////////////////////////
        /// \brief Update window flags
        ///
        /// \param flags New Flags
        /////////////////////////////
        void UpdateFlags(uint32_t flags);

        /////////////////////////////
        /// \brief Paint window
        ///
        /// Call OnPaint(), if relevant draw GUI Widgets, then swap the window buffers.
        /////////////////////////////
        void Paint();

        /////////////////////////////
        /// \brief Swap the window buffers
        ///
        /// Swap the window buffers, equivalent to Paint() on a Basic Window without OnPaint()
        /////////////////////////////
        void SwapBuffers();

        /////////////////////////////
        /// \brief Poll the window for events
        ///
        /// Fill ev with an event if an event has been found and return true, otherwise return false
        ///
        /// \param ev Reference to \a LemonEvent to fill 
        /// 
        /// \return true when an event has been received, false when there was no event
        /////////////////////////////
        bool PollEvent(LemonEvent& ev);

        /////////////////////////////
        /// \brief Wait for an event
        ///
        /// Blocks the thread until an event has been recieved
        /////////////////////////////
        void WaitEvent();
        
        /////////////////////////////
        /// \brief Send event
        ///
        /// Send an event for the root container widget (and its children) to handle.
        /// If the application decides to use the GUI they can pass events from PollEvent to here
        ///
        /// \param ev Event to send
        /////////////////////////////
        void GUIHandleEvent(LemonEvent& ev);

        /////////////////////////////
        /// \brief Add a widget to the window
        ///
        /// Adds a widget to the list of widgets in the root container. Pointer should be valid until Widget is removed.
        ///
        /// \param w Pointer to widget
        /////////////////////////////
        void AddWidget(Widget* w);
        
        /////////////////////////////
        /// \brief Remove a widget from the window
        ///
        /// Removes a widget to the list of widgets in the root container. Always do this first before destroying the Widget.
        ///
        /// \param w Pointer to widget
        /////////////////////////////
        void RemoveWidget(Widget* w);

        /////////////////////////////
        /// \brief Display a context menu on screen
        ///
        /// Get the window manager to display a context menu on the screen. An EventWindowCommand will be returned if/when an item in the context menu is clicked.
        /// When pos is {x: -1, y: -1} the context menu will be displayed around the location of the mouse cursor
        ///
        /// \param entries Entries (name and id pair) to display
        /// \param pos Position of context menu
        /////////////////////////////
        void DisplayContextMenu(const std::vector<ContextMenuEntry>& entries, vector2i_t pos = {-1, -1});

        /////////////////////////////
        /// \brief Initialize the window menu bar
        ///
        /// Creates MenuBar object and adjusts root container accordingly.
        /////////////////////////////
        void CreateMenuBar();

        void (*OnMenuCmd)(unsigned short, Window*) = nullptr;
        
        /////////////////////////////
        /// \brief Get Window Flags
        ///
        /// \return window flags
        /////////////////////////////
        inline uint32_t GetFlags() const { return flags; }

        /////////////////////////////
        /// \brief Get Window Size
        ///
        /// \return window size
        /////////////////////////////
        inline vector2i_t GetSize() const { return {surface.width, surface.height}; };

        /////////////////////////////
        /// \brief OnPaint callback
        ///
        /// Called when Paint() is called. Is run \e before the widgets are painted and the window buffers are swapped
        /////////////////////////////
        WindowPaintHandler OnPaint = nullptr;
    };
}