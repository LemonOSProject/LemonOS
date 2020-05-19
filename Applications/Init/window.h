#pragma once

#include <gui/window.h>

#define WINDOW_BORDER_COLOUR {32,32,32}
#define WINDOW_TITLEBAR_HEIGHT 24

class Window{
public:
	handle_t handle;
	win_info_t info;
	vector2i_t pos;
	uint8_t* primaryBuffer;
	uint8_t* secondaryBuffer;
	bool active = false;

	void DrawWindow(surface_t* surface);
	rect_t GetRect();
};