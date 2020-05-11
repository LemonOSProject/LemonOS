#include <stdint.h>

#include <lemon/types.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <lemon/sharedmem.h>
#include <unistd.h>

struct Surface;

namespace Lemon::GUI{

	handle_t _CreateWindow(win_info_t* wininfo){
		handle_t h;

		syscall(SYS_CREATE_WINDOW,(uintptr_t)wininfo, 0,0,0,0);

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

		info->primaryBufferKey = Lemon::CreateSharedMemory((info->width * 4) * info->height, SMEM_FLAGS_PRIVATE, getpid(), syscall(SYS_GET_DESKTOP_PID, 0, 0 ,0, 0, 0));
		info->secondaryBufferKey = Lemon::CreateSharedMemory((info->width * 4) * info->height, SMEM_FLAGS_PRIVATE, getpid(), syscall(SYS_GET_DESKTOP_PID, 0, 0 ,0, 0, 0));

		win->primaryBuffer = (uint8_t*)Lemon::MapSharedMemory(info->primaryBufferKey);
		win->secondaryBuffer = (uint8_t*)Lemon::MapSharedMemory(info->secondaryBufferKey);

		handle_t handle = _CreateWindow(info);
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

		Lemon::UnmapSharedMemory(win->primaryBuffer, win->info.primaryBufferKey);
		Lemon::DestroySharedMemory(win->info.primaryBufferKey);
		Lemon::UnmapSharedMemory(win->secondaryBuffer, win->info.secondaryBufferKey);
		Lemon::DestroySharedMemory(win->info.secondaryBufferKey);
		delete win;
	}

	void UpdateWindow(Window* win){
		syscall(SYS_UPDATE_WINDOW, win->handle, 0, &win->info, 0, 0);
	}

	void SwapWindowBuffers(Window* win){
		if(win->surface.buffer == win->primaryBuffer){
			win->surface.buffer = win->secondaryBuffer;
			win->info.currentBuffer = 0;
		} else {
			win->surface.buffer = win->primaryBuffer;
			win->info.currentBuffer = 1;
		}
		UpdateWindow(win);
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
		win->activeWidget = -1;

		for(int i = 0; i < win->widgets.get_length(); i++){
			rect_t widgetBounds = win->widgets.get_at(i)->bounds;
			if(Graphics::PointInRect(widgetBounds, mousePos)){
				win->widgets.get_at(i)->OnMouseDown(mousePos);
				win->lastPressedWidget = i;
				win->activeWidget = i;
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

	void HandleKeyPress(Window* win, int key){
		if(win->activeWidget > -1){
			win->widgets[win->activeWidget]->OnKeyPress(key);
		}
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