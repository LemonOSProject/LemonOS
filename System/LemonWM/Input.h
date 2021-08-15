#pragma once

#include <Lemon/Graphics/Vector.h>

struct MouseState {
    Vector2i pos;
    bool left, middle, right;
};

struct KeyboardState{
	bool caps, control, shift, alt;
};

class InputManager{
public:
    MouseState mouse = {{100, 100}, false, false, false};
    KeyboardState keyboard = {false, false, false, false};

    InputManager(const Vector2i& mouseBounds);

    void Poll();

private:
    Vector2i m_mouseBounds; // Maximum mouse cursor position
};