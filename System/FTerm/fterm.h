#pragma once

#include <Lemon/Core/Keyboard.h>
#include <Lemon/Graphics/Graphics.h>

struct KeyboardState{
	bool caps, control, shift, alt;
};

void OnKey(int key);

class InputManager{

public:
    KeyboardState keyboard;

    void Poll();
};