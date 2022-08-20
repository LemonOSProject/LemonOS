#pragma once

#include <lemon/core/Keyboard.h>
#include <lemon/graphics/Graphics.h>

struct KeyboardState{
	bool caps, control, shift, alt;
};

void OnKey(int key);

class InputManager{

public:
    KeyboardState keyboard;

    void Poll();
};