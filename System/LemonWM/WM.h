#pragma once

#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Surface.h>
#include <Lemon/Graphics/Types.h>

#include <Lemon/IPC/Endpoint.h>
#include <Lemon/IPC/Interface.h>

#include <Lemon/Services/lemon.lemonwm.h>

#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Event.h>

#include <list>

#include "WindowRect.h"

#define WINDOW_BORDER_COLOUR (RGBAColour{32,32,32})
#define WINDOW_TITLEBAR_HEIGHT 24
#define WINDOW_BORDER_THICKNESS 2
#define CONTEXT_ITEM_HEIGHT 20
#define CONTEXT_ITEM_WIDTH 160

#define LEMONWM_MSG_SIZE 512

using WindowBuffer = Lemon::GUI::WindowBuffer;

inline long operator-(const timespec& t1, const timespec& t2){
    return (t1.tv_sec - t2.tv_sec) * 1000000000 + (t1.tv_nsec - t2.tv_nsec);
}

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

class WMWindow
    : public LemonWMClientEndpoint {
    friend class CompositorInstance;

    WMInstance* wm;
public:
    WMWindow(WMInstance* wm, const Lemon::Handle& client, int64_t key, WindowBuffer* bufferInfo, vector2i_t pos, vector2i_t size, unsigned int flags, const std::string& title);
    ~WMWindow();

    vector2i_t pos = {0, 0};
    vector2i_t size = {0, 0};
    std::string title;
    uint32_t flags = 0;
    bool minimized = false;

    short closeBState = ButtonStateUp; 
    short minimizeBState = ButtonStateUp;

    int64_t windowID = 0;

    inline const Lemon::Handle& GetClient() { return LemonWMClientEndpoint::m_handle; }
    inline int64_t ID() const { return windowID; }

    void DrawDecoration(surface_t* surface, rect_t) const;
    void DrawClip(surface_t* surface, rect_t clip);
    //void Draw(surface_t* surface);

    inline void SendEvent(const Lemon::LemonEvent& ev) {
        if(!(flags & WINDOW_FLAGS_TOOLTIP))
            LemonWMClientEndpoint::SendEvent(windowID, ev.event, ev.data);
    }

    void Minimize(bool state);
    void Resize(vector2i_t size, int64_t bufferKey, WindowBuffer* bufferInfo);

    inline int Dirty() const { return windowBufferInfo->dirty; }
    inline void SetDirty(int v) { windowBufferInfo->dirty = v; }

    rect_t GetWindowRect() const; // Return window bounds as a rectangle

    rect_t GetCloseRect() const;
    rect_t GetMinimizeRect() const;

    rect_t GetBottomBorderRect() const;
    rect_t GetTopBorderRect() const;
    rect_t GetLeftBorderRect() const;
    rect_t GetRightBorderRect() const;

    void RecalculateButtonRects(); // Recalculate rectangles for close/minimize buttons
    
protected:
    int64_t sharedBufferKey;

    WindowBuffer* windowBufferInfo;
    uint8_t* buffer1;
    uint8_t* buffer2;
    int64_t bufferKey;

    rect_t closeRect, minimizeRect;

    surface_t windowSurface;
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

    std::list<WMWindowRect> clips;
public:
    CompositorInstance(WMInstance* wm);

    void RecalculateClipping();
    void Paint();

    surface_t windowButtons;
    surface_t mouseCursor;
    surface_t mouseBuffer = { .width = 0, .height = 0, .depth = 32, .buffer = nullptr };
    vector2i_t lastMousePos;

    bool displayFramerate = false;
    bool useImage = true;
    surface_t backgroundImage;
};

class WMInstance final
    : LemonWMServer {
protected:
    friend void* _InitializeShellConnection(void*);
    friend class WMWindow;

    int64_t nextWindowID = 1;
    
    Lemon::Interface server;
    Lemon::Endpoint shellClient;

    WMWindow* active = nullptr;
    WMWindow* lastMousedOver = nullptr;

    bool drag = false;
    bool resize = false;
    vector2i_t dragOffset;
    int resizePoint = Right;
    vector2i_t resizeStartPos;

    bool shellConnected = false;

    void* InitializeShellConnection();
    void Poll();
    void PostEvent(const Lemon::LemonEvent& ev, WMWindow* win);

    WMWindow* FindWindow(int64_t id);
    WMWindow* FindWindowByClient(const Lemon::Handle& clientID);
    int64_t NextWindowID();

    void MinimizeWindow(WMWindow* win, bool state);
    void MinimizeWindow(int id, bool state);

    void SetActive(WMWindow* win);
    void DestroyWindow(WMWindow* win);

    int64_t CreateWindowBuffer(int width, int height, WindowBuffer** buffer);

    void OnCreateWindow(const Lemon::Handle& client, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t flags, const std::string& title) override;
    void OnDestroyWindow(const Lemon::Handle& client, int64_t windowID) override;
    void OnSetTitle(const Lemon::Handle& client, int64_t windowID, const std::string& title) override;
    void OnUpdateFlags(const Lemon::Handle& client, int64_t windowID, uint32_t flags) override;
    void OnRelocate(const Lemon::Handle& client, int64_t windowID, int32_t x, int32_t y) override;
    void OnGetPosition(const Lemon::Handle& client, int64_t windowID) override;
    void OnResize(const Lemon::Handle& client, int64_t windowID, int32_t width, int32_t height) override;
    void OnMinimize(const Lemon::Handle& client, int64_t windowID, bool minimized) override;
    void OnDisplayContextMenu(const Lemon::Handle& client, int64_t windowID, int32_t x, int32_t y, const std::string& entries) override;
    void OnPong(const Lemon::Handle& client, int64_t windowID) override;

    void OnPeerDisconnect(const Lemon::Handle& client) override;
    void OnGetScreenBounds(const Lemon::Handle& client) override;
    void OnReloadConfig(const Lemon::Handle& client) override;

public:
    timespec startTime;
    timespec endTime;

    bool redrawBackground = true;
    bool contextMenuActive = false;
    rect_t contextMenuBounds;

    long targetFrameDelay = 8333; // 8333 us (120 fps)
    long frameDelayThreshold = 8333 - (8333 / 2); // -50% of the frame delay

    surface_t surface;
    surface_t screenSurface;

    std::list<WMWindow*> windows;
    std::string themePath = "/system/lemon/themes/classic.json";

    InputManager input = InputManager(this);
    CompositorInstance compositor = CompositorInstance(this);
    ContextMenu menu;

    WMInstance(surface_t& surface, const Lemon::Handle& svc, const char* ifName);
    
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