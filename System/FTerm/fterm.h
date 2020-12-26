#pragma once

#include <lemon/core/keyboard.h>
#include <lemon/gfx/graphics.h>

struct KeyboardState{
	bool caps, control, shift, alt;
};

void OnKey(int key);

class InputManager{

public:
    KeyboardState keyboard;

    void Poll();
};