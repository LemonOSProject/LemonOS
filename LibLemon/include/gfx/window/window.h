#pragma once

#define WINDOW_FLAGS_NODECORATION 0x1
#define WINDOW_FLAGS_SNAP_TO_BOTTOM 0x2
#define WINDOW_FLAGS_MINIMIZED 0x4

#include <stdint.h>
#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/window/widgets.h>
#include <list.h>

typedef struct {
	uint16_t x = 0;
	uint16_t y = 0;

	uint16_t width = 0; // Width
	uint16_t height = 0; // Height

	uint32_t flags; // Window Flags

	char title[96]; // Title of window

	uint64_t ownerPID;

	handle_t handle;
} __attribute__((packed)) win_info_t;

namespace Lemon::GUI{
	typedef void(*WindowPaintHandler)(surface_t*);


	struct Window{
		List<Widget*> widgets;

		handle_t handle = (handle_t)0;
		win_info_t info;

		rgba_colour_t background = {192,192,190,255};

		surface_t surface;
		uint8_t* primaryBuffer;
		uint8_t* secondaryBuffer;

		int lastPressedWidget = -1;
		int activeWidget = -1;
		vector2i_t mousePos;
		
		WindowPaintHandler OnPaint = NULL;
	};

	//handle_t _CreateWindow(win_info_t* wininfo);
	handle_t _CreateWindow(win_info_t* wininfo, void** primaryBuffer, void** secondaryBuffer);
	void _DestroyWindow(handle_t window);
	void _PaintWindow(handle_t window);//, surface_t* surface);
	void SwapWindowBuffers(Window* win);

	Window* CreateWindow(win_info_t* info);
	void DestroyWindow(Window* win);
	void ResizeWindow(Window* win, vector2i_t size);
	void PaintWindow(Window* win);
	void UpdateWindow(Window* win);

	void HandleMouseDown(Window* win, vector2i_t mousePos);
	Widget* HandleMouseUp(Window* win, vector2i_t mousePos);
	void HandleMouseMovement(Window* win, vector2i_t mousePos);
	void HandleKeyPress(Window* win, int key);

	void AddWidget(Widget* widget, Window* win);
}