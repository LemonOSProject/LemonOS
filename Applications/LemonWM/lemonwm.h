#pragma once

#include <gfx/graphics.h>
#include <gfx/surface.h>

#include <core/msghandler.h>
#include <core/event.h>
#include <gui/window.h>

#include <list>

#define WINDOW_BORDER_COLOUR {32,32,32}
#define WINDOW_TITLEBAR_HEIGHT 24

using WindowBuffer = Lemon::GUI::WindowBuffer;

class WMWindow {
protected:
    unsigned long sharedBufferKey;

    WindowBuffer* windowBufferInfo;
    uint8_t* buffer1;
    uint8_t* buffer2;
public:
    WMWindow(unsigned long key);

    vector2i_t pos;
    vector2i_t size;
    char* title;
    uint32_t flags;

    int clientFd;

    void Draw(surface_t* surface);
};

struct MouseState {
    vector2i_t pos;
    bool left, middle, right;
};

struct KeyboardState{
	bool caps, control, shift, alt;
};

class WMInstance;

class InputManager{
protected:
    WMInstance* wm;

public:
    MouseState mouse;
    KeyboardState keyboard;

    InputManager(WMInstance* wm);
    void Poll();
};

class CompositorInstance{
protected:
    WMInstance* wm;

public:
    CompositorInstance(WMInstance* wm);
    void Paint();
};

class WMInstance {
protected:
    Lemon::MessageServer server;

    WMWindow* active;
    bool drag;
    vector2i_t dragOffset;

    void Poll();
    void PostEvent(Lemon::LemonEvent& ev, WMWindow* win);
public:
    surface_t surface;

    std::list<WMWindow*> windows;

    InputManager input = InputManager(this);
    CompositorInstance compositor = CompositorInstance(this);

    WMInstance(surface_t& surface, sockaddr_un address);
    
    void Update();

    void MouseDown();
    void MouseUp();
    void KeyUpdate(int key, bool pressed);
};

static inline bool PointInWindow(WMWindow* win, vector2i_t point){
	int windowHeight = (win->flags & WINDOW_FLAGS_NODECORATION) ? win->size.y : (win->size.y + 25); // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos},{win->size.x, windowHeight}}, point);
}

static inline bool PointInWindowProper(WMWindow* win, vector2i_t point){
	int windowYOffset = (win->flags & WINDOW_FLAGS_NODECORATION) ? 0 : 25; // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos + (vector2i_t){0, windowYOffset}},{win->size.x, win->size.y}}, point);
}