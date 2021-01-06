#pragma once

#include <lemon/gfx/graphics.h>
#include <lemon/gfx/surface.h>

#include <lemon/ipc/endpoint.h>
#include <lemon/ipc/interface.h>
#include <lemon/core/event.h>
#include <lemon/gui/window.h>

#include <list>

#define WINDOW_BORDER_COLOUR {32,32,32}
#define WINDOW_TITLEBAR_HEIGHT 24
#define WINDOW_BORDER_THICKNESS 2
#define CONTEXT_ITEM_HEIGHT 20
#define CONTEXT_ITEM_WIDTH 160

//#define LEMONWM_USE_CLIPPING
#define LEMONWM_MSG_SIZE 512

using WindowBuffer = Lemon::GUI::WindowBuffer;

class WMInstance;

enum WMButtonState{
    ButtonStateUp,
    ButtonStateHover,
    ButtonStatePressed,
};

enum ResizePoint {
    Top,
    Bottom,
    Left,
    Right,
    BottomLeft,
    BottomRight,
    TopLeft,
    TopRight,
};

class WMWindow : Lemon::Endpoint {
    friend class CompositorInstance;
protected:
    int64_t sharedBufferKey;

    WindowBuffer* windowBufferInfo;
    uint8_t* buffer1;
    uint8_t* buffer2;
    int64_t bufferKey;

    WMInstance* wm;

    rect_t closeRect, minimizeRect;
public:
    WMWindow(WMInstance* wm, handle_t endp, handle_t id, int64_t key, WindowBuffer* bufferInfo, vector2i_t pos, vector2i_t size, unsigned int flags, const std::string& title);
    ~WMWindow();

    #ifdef LEMONWM_USE_CLIPPING
        std::list<rect_t> clips;
    #endif

    vector2i_t pos = {0, 0};
    vector2i_t size = {0, 0};
    std::string title;
    uint32_t flags = 0;
    bool minimized = false;

    short closeBState = ButtonStateUp; 
    short minimizeBState = ButtonStateUp;

    handle_t clientID = 0;

    void Draw(surface_t* surface);

    void PostEvent(Lemon::LemonEvent& ev) { Queue(Lemon::GUI::WindowEvent, reinterpret_cast<uintptr_t>(&ev), (uint16_t)sizeof(Lemon::LemonEvent)); }

    void Minimize(bool state);
    void Resize(vector2i_t size, int64_t bufferKey, WindowBuffer* bufferInfo);
    void RecalculateRects();

    rect_t GetCloseRect();
    rect_t GetMinimizeRect();

    rect_t GetBottomBorderRect();
    rect_t GetTopBorderRect();
    rect_t GetLeftBorderRect();
    rect_t GetRightBorderRect();

    void RecalculateButtonRects();
};

class ContextMenuItem{
public:
    std::string name;
    unsigned char index;
    unsigned short id;

    ContextMenuItem(const std::string& name, unsigned char index, unsigned short id){
        this->name = name;
        this->index = index;
        this->id = id;
    }
};

class ContextMenu{
public:
    std::vector<ContextMenuItem> items;
    WMWindow* owner;

    void Paint(surface_t surface);
};

struct MouseState {
    vector2i_t pos;
    bool left, middle, right;
};

struct KeyboardState{
	bool caps, control, shift, alt;
};

class InputManager{
protected:
    WMInstance* wm;

public:
    MouseState mouse = {{100, 100}, false, false, false};
    KeyboardState keyboard = {false, false, false, false};

    InputManager(WMInstance* wm);
    void Poll();
};

class CompositorInstance{
protected:
    WMInstance* wm;

    timespec lastRender = {0, 0};

    #ifdef LEMONWM_USE_CLIPPING
        std::list<rect_t> cclips;
    #endif
public:
    CompositorInstance(WMInstance* wm);
    void Paint();

    surface_t windowButtons;
    surface_t mouseCursor;

    bool capFramerate = false;
    bool displayFramerate = false;
    bool useImage = true;
    surface_t backgroundImage;
};

class WMInstance{
protected:
    friend void* _InitializeShellConnection(void*);
    
    Lemon::Interface server;
    Lemon::Endpoint shellClient;

    WMWindow* active = nullptr;
    bool drag = false;
    bool resize = false;
    vector2i_t dragOffset;
    int resizePoint = Right;
    vector2i_t resizeStartPos;

    bool shellConnected = false;

    void* InitializeShellConnection();
    void Poll();
    void PostEvent(Lemon::LemonEvent& ev, WMWindow* win);
    WMWindow* FindWindow(int id);

    void MinimizeWindow(WMWindow* win, bool state);
    void MinimizeWindow(int id, bool state);

    void SetActive(WMWindow* win);

    int64_t CreateWindowBuffer(int width, int height, WindowBuffer** buffer);
public:
    bool redrawBackground = true;
    bool contextMenuActive = false;
    rect_t contextMenuBounds;

    surface_t surface;
    surface_t screenSurface;

    std::list<WMWindow*> windows;
    std::string themePath = "/system/lemon/themes/classic.json";

    InputManager input = InputManager(this);
    CompositorInstance compositor = CompositorInstance(this);
    ContextMenu menu;

    WMInstance(surface_t& surface, handle_t svc, const char* ifName);
    
    void Update();

    void MouseDown();
    void MouseRight(bool pressed);
    void MouseUp();
    void MouseMove();
    void KeyUpdate(int key, bool pressed);
};

static inline bool PointInWindow(WMWindow* win, vector2i_t point){
	int windowHeight = (win->flags & WINDOW_FLAGS_NODECORATION) ? win->size.y : (win->size.y + WINDOW_TITLEBAR_HEIGHT + (WINDOW_BORDER_THICKNESS * 4)); // Account for titlebar and borders
	int windowWidth = (win->flags & WINDOW_FLAGS_NODECORATION) ? win->size.x : (win->size.x + (WINDOW_BORDER_THICKNESS * 4)); // Account for borders and extend the window a little bit so it is easier to resize

    vector2i_t windowOffset = (win->flags & WINDOW_FLAGS_NODECORATION) ? (vector2i_t){0, 0} : (vector2i_t){-WINDOW_BORDER_THICKNESS, -WINDOW_BORDER_THICKNESS};

	return Lemon::Graphics::PointInRect({{win->pos + windowOffset},{windowWidth, windowHeight}}, point);
}

static inline bool PointInWindowProper(WMWindow* win, vector2i_t point){
	int windowYOffset = (win->flags & WINDOW_FLAGS_NODECORATION) ? 0 : (WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS); // Account for titlebar
    int windowXOffset = (win->flags & WINDOW_FLAGS_NODECORATION) ? 0 : (WINDOW_BORDER_THICKNESS); // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos + (vector2i_t){windowXOffset, windowYOffset}},{win->size.x, win->size.y}}, point);
}