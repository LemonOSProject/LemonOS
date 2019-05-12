#pragma once

#define WINDOW_FLAGS_NODECORATION 0x1
#define WINDOW_FLAGS_SNAP_TO_BOTTOM 0x2

#include <stdint.h>
#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/window/widgets.h>
#include <list.h>

typedef struct {
	uint16_t x;
	uint16_t y;

	uint16_t width; // Width
	uint16_t height; // Height

	uint32_t flags; // Window Flags

	char title[96]; // Title of window

	uint64_t ownerPID;

	handle_t handle;
} __attribute__((packed)) win_info_t;

class Window{
public:
	List<Widget*> widgets;

	handle_t handle;
	win_info_t info;

	rgba_colour_t background = {192,192,190,255};

	surface_t surface;

	int lastPressedWidget;

	void AddWidget(Widget* widget){
		widgets.add_back(widget);
	}
};

Window* CreateWindow(win_info_t* info);
void DestroyWindow(Window* win);
void PaintWindow(Window* win);

void HandleMouseDown(Window* win, vector2i_t mousePos);
void HandleMouseUp(Window* win);