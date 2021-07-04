#pragma once

#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Surface.h>
#include <Lemon/GUI/CtxEntry.h>
#include <Lemon/GUI/Event.h>
#include <Lemon/GUI/Widgets.h>
#include <Lemon/GUI/WindowServer.h>

#include <utility>
#include <queue>

#define WINDOW_FLAGS_NODECORATION 0x1 // Do not draw window borders
#define WINDOW_FLAGS_RESIZABLE 0x2 // Allow window resizing
#define WINDOW_FLAGS_NOSHELL 0x4 // Do not show up in shell events
#define WINDOW_FLAGS_TOOLTIP 0x8 // Don't receive events
#define WINDOW_FLAGS_ALWAYS_ACTIVE 0x10 // Hide window when inactive

#define WINDOW_MENUBAR_HEIGHT 20

namespace Lemon::GUI {
	typedef void(*WindowPaintHandler)(surface_t*);

    enum {
        WMCxtEntryTypeCommand,
        WMCxtEntryTypeDivider,
        WMCxtEntryTypeExpand,
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

    class Window final {
        friend class Lemon::WindowServer;
    public:
        Window(const char* title, vector2i_t size, uint32_t flags = 0, int type = WindowType::Basic, vector2i_t pos = {20, 20});
        ~Window();

        /////////////////////////////
        /// \brief Set title of window
        ///
        /// \param name Service name to be used, must be unique
        /////////////////////////////
        void SetTitle(const std::string& title);
        
        /////////////////////////////
        /// \brief Relocate window
        ///
        /// Move the window to position (x, y) on screen
        ///
        /// \param pos New position of window
        /////////////////////////////
        void Relocate(vector2i_t pos);

        /////////////////////////////
        /// \brief Resize window
        ///
        /// Resize the window
        ///
        /// \param size New size of window
        /////////////////////////////
        void Resize(vector2i_t size);
        
        /////////////////////////////
        /// \brief Minimize a window
        ///
        /// Show/hide the specified window on screen 
        ///
        /// \param windowID Window to show/hide
        /// \param minimized Whether to show/hide the window
        /////////////////////////////
        void Minimize(int windowID, bool minimized);

        /////////////////////////////
        /// \brief Minimize window
        ///
        /// Show/hide the window on screen 
        ///
        /// \param minimized Whether to show/hide the window
        /////////////////////////////
        inline void Minimize(bool minimized = true) { Minimize(ID(), minimized); }

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
        virtual void Paint();

        /////////////////////////////
        /// \brief Swap the window buffers
        ///
        /// Swap the window buffers, equivalent to Paint() on a Basic Window without OnPaint()
        /////////////////////////////
        void SwapBuffers();

        /////////////////////////////
        /// \brief Check the event queue for events.
        ///
        /// Fill ev with an event if an event has been found and return true, otherwise return false
        ///
        /// \param ev Reference to \a LemonEvent to fill 
        /// 
        /// \return true when an event has been received, false when there was no event
        /////////////////////////////
        bool PollEvent(LemonEvent& ev);
        
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
        /// \brief Set widget as active
        ///
        /// \param w Pointer to widget
        /////////////////////////////
        void SetActive(Widget* w);

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
        /// \brief Set toolip
        ///
        /// \param text Tooltip text
        /// \param pos Position (absolute) of tooltip
        /////////////////////////////
        void SetTooltip(const char* text, vector2i_t pos);

        /////////////////////////////
        /// \brief Hide toolip
        /////////////////////////////
        inline void HideTooltip() { if(tooltipWindow.get()) tooltipWindow->Minimize(true); }
        
        /////////////////////////////
        /// \brief Get Window Flags
        ///
        /// \return window flags
        /////////////////////////////
        inline uint32_t GetFlags() const { return flags; }

        /////////////////////////////
        /// \brief Get Window ID
        ///
        /// \return window id
        /////////////////////////////
        inline long ID() const { return windowID; }

        /////////////////////////////
        /// \brief Get Window Size
        ///
        /// \return window size
        /////////////////////////////
        inline vector2i_t GetSize() const { return {surface.width, surface.height}; };

        /////////////////////////////
        /// \brief Get Window Position
        ///
        /// \return window position
        /////////////////////////////
        vector2i_t GetPosition();

        /////////////////////////////
        /// \brief OnPaint callback
        ///
        /// Called when Paint() is called. Is run \e before the widgets are painted and the window buffers are swapped
        /////////////////////////////
        WindowPaintHandler OnPaint = nullptr;

        /////////////////////////////
        /// \brief OnPaint callback
        ///
        /// Called when Paint() is called. Is run \e after the widgets are painted and the window buffers are swapped
        /////////////////////////////
        WindowPaintHandler OnPaintEnd = nullptr;

        vector2i_t lastMousePos = {-1, -1};
        WindowMenuBar* menuBar = nullptr;
        Widget* tooltipOwner = nullptr;
        Container rootContainer;
        surface_t surface = {0, 0, 32, nullptr};
        bool closed = false; // Set to true when close button pressed

    private:
        class TooltipWindow 
            : protected LemonWMServerEndpoint {
        public:
            TooltipWindow(const char* text, vector2i_t pos, const RGBAColour& bgColour);

            void SetText(const char* text);
            inline void Relocate(vector2i_t pos) { LemonWMServerEndpoint::Relocate(windowID, pos.x, pos.y); }

            void Paint();

            inline void Minimize(bool minimized) { LemonWMServerEndpoint::Minimize(windowID, minimized); }
        
        private:
            int64_t windowID = 0;

            Graphics::TextObject textObject;
            RGBAColour backgroundColour;

            WindowBuffer* windowBufferInfo;
            uint8_t* buffer1;
            uint8_t* buffer2;
            uint64_t windowBufferKey;

            surface_t surface = {0, 0, 32, nullptr};
        };

        int64_t windowID = 0;
        
        WindowBuffer* windowBufferInfo = nullptr;
        uint8_t* buffer1;
        uint8_t* buffer2;
        uint64_t windowBufferKey;

        uint32_t flags;

        int windowType = WindowType::Basic;

        timespec lastClick = {0, 0};

        std::unique_ptr<TooltipWindow> tooltipWindow = nullptr;
        std::queue<Lemon::LemonEvent> eventQueue;
    };
}