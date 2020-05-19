#pragma once

#include "window.h"

#include <lemon/ipc.h>

struct KeyboardState{
	bool caps, control, shift, alt;
};

int HandleKeyEvent(ipc_message_t& msg, Window* active);