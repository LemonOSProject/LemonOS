#include <stdint.h>

#include <lemon/types.h>
#include <gfx/window/window.h>
#include <stdlib.h>

//#include <runtime.h>

struct Surface;

namespace Lemon::GUI{
	handle_t _CreateWindow(win_info_t* wininfo){
		handle_t h;
		syscall(SYS_CREATE_WINDOW,(uintptr_t)wininfo,0,0,0,0);

		h = wininfo->handle;

		return h;
	}

	handle_t _CreateWindow(win_info_t* wininfo, void** primaryBuffer, void** secondaryBuffer){
		handle_t h;
		syscall(SYS_CREATE_WINDOW,(uintptr_t)wininfo, primaryBuffer,secondaryBuffer,0,0);

		h = wininfo->handle;

		return h;
	}

	void _DestroyWindow(handle_t window){
		syscall(SYS_DESTROY_WINDOW, (uintptr_t)window, 0, 0, 0, 0);
	}

	void _PaintWindow(handle_t window, int buffer){
		syscall(SYS_UPDATE_WINDOW, (uintptr_t)window, buffer, 0, 0, 0);
	}

	Window* CreateWindow(win_info_t* info){

		Window* win = new Window();

		handle_t handle = _CreateWindow(info, (void**)&win->primaryBuffer, (void**)&win->secondaryBuffer);
		win->handle = handle;
		memcpy(&win->info,info,sizeof(win_info_t));

		surface_t surface;
		surface.width = info->width;
		surface.height = info->height;

		surface.buffer = (uint8_t*)win->primaryBuffer;//(uint8_t*)malloc((surface.width * 4) * info->height);

		surface.linePadding = 0;

		win->surface = surface;

		return win;
	}

	void DestroyWindow(Window* win){
		_DestroyWindow(win->handle);
		delete win;
	}

	void UpdateWindow(Window* win){
		syscall(SYS_UPDATE_WINDOW, win->handle, 0, &win->info, 0, 0);
	}

	void SwapWindowBuffers(Window* win){
		if(win->surface.buffer == win->primaryBuffer){
			win->surface.buffer = win->secondaryBuffer;
			_PaintWindow(win->handle, 1);
		} else {
			win->surface.buffer = win->primaryBuffer;
			_PaintWindow(win->handle, 2);
		}
	}

	void PaintWindow(Window* win){
		//if(!(win->info.flags & WINDOW_FLAGS_NOBACKGROUND))
			Graphics::DrawRect(0,0,win->info.width, win->info.height, win->background, &win->surface);

		for(int i = 0; i < win->widgets.get_length(); i++){
			win->widgets.get_at(i)->Paint(&win->surface);
		}

		if(win->OnPaint){
			win->OnPaint(&win->surface);
		}

		SwapWindowBuffers(win);
	}

	void HandleMouseDown(Window* win, vector2i_t mousePos){
		for(int i = 0; i < win->widgets.get_length(); i++){
			rect_t widgetBounds = win->widgets.get_at(i)->bounds;
			if(Graphics::PointInRect(widgetBounds, mousePos)){
				win->widgets.get_at(i)->OnMouseDown(mousePos);
				win->lastPressedWidget = i;
				break;
			}
		}
	}

	Widget* HandleMouseUp(Window* win, vector2i_t mousePos){
		if(win->lastPressedWidget >= 0){
			win->widgets[win->lastPressedWidget]->OnMouseUp(mousePos);
			return win->widgets[win->lastPressedWidget];
		}
		win->lastPressedWidget = -1;
		return NULL;
	}

	void HandleMouseMovement(Window* win, vector2i_t mousePos){
		win->mousePos = mousePos;

		if(win->lastPressedWidget >= 0){
			win->widgets[win->lastPressedWidget]->OnMouseMove(mousePos);
		}else for(int i = 0; i < win->widgets.get_length(); i++){
			rect_t widgetBounds = win->widgets.get_at(i)->bounds;
			if(Graphics::PointInRect(widgetBounds, mousePos)){
				win->widgets.get_at(i)->OnHover(mousePos);
				break;
			}
		}
	}

	void AddWidget(Widget* widget, Window* win){
		win->widgets.add_back(widget);
	}
}