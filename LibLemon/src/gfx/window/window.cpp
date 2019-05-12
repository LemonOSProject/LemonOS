#include <stdint.h>

#include <core/types.h>
#include <gfx/window/window.h>

#include <runtime.h>

struct Surface;

handle_t _CreateWindow(win_info_t* wininfo){
	handle_t h;
	syscall(SYS_CREATE_WINDOW,(uintptr_t)&h,(uintptr_t)wininfo,0,0,0);
	return h;
}

void _DestroyWindow(handle_t window){
	syscall(SYS_DESTROY_WINDOW, (uintptr_t)window, 0, 0, 0, 0);
}

void _PaintWindow(handle_t window, surface_t* surface){
	syscall(SYS_PAINT_WINDOW, (uintptr_t)window, (uintptr_t)surface, 0, 0, 0);
}

Window* CreateWindow(win_info_t* info){
	handle_t handle = _CreateWindow(info);

	Window* win = new Window();
	win->handle = handle;
	win->info = *info;

	surface_t surface;
	surface.width = info->width;
	surface.height = info->height;
	surface.buffer = (uint8_t*)malloc(surface.width * surface.height * 4);

	win->surface = surface;

	return win;
}

void DestroyWindow(Window* win){
	_DestroyWindow(win->handle);

	delete win;
}

void PaintWindow(Window* win){
	DrawRect(0,0,win->info.width, win->info.height, win->background, &win->surface);
	for(int i = 0; i < win->widgets.get_length(); i++){
		win->widgets[i]->Paint(&win->surface);
	}

	_PaintWindow(win->handle, &win->surface);
}

void HandleMouseDown(Window* win, vector2i_t mousePos){
	for(int i = 0; i < win->widgets.get_length(); i++){
		rect_t widgetBounds = win->widgets[i]->bounds;
		if(widgetBounds.pos.x < mousePos.x && widgetBounds.pos.y < mousePos.y && widgetBounds.pos.x + widgetBounds.size.x > mousePos.x && widgetBounds.pos.y + widgetBounds.size.y > mousePos.y){
			win->widgets[i]->OnMouseDown();
			win->lastPressedWidget = i;
			break;
		}
	}
}

void HandleMouseUp(Window* win){
	win->widgets[win->lastPressedWidget]->OnMouseUp();
}