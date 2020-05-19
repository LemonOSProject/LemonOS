#include "window.h"

extern surface_t closeButtonSurface;

void Window::DrawWindow(surface_t* surface){
	if(info.flags & WINDOW_FLAGS_MINIMIZED){
		return;
	}

	if(info.flags & WINDOW_FLAGS_NODECORATION){
		syscall(SYS_DESKTOP_GET_WINDOW, &info, info.handle, 0, 0, 0); // Update the current window buffer
		surface_t wSurface = {.width = info.width, .height = info.height, .buffer = (info.currentBuffer ? secondaryBuffer : primaryBuffer)};
		Lemon::Graphics::surfacecpy(surface, &wSurface, pos);
		return;
	}

	Lemon::Graphics::DrawRectOutline(pos.x, pos.y, info.width + 2, info.height + WINDOW_TITLEBAR_HEIGHT + 2, WINDOW_BORDER_COLOUR, surface);
	Lemon::Graphics::DrawGradientVertical({pos + (vector2i_t){1,1}, {info.width, WINDOW_TITLEBAR_HEIGHT}}, {96, 96, 96}, {42, 50, 64}, surface);

	Lemon::Graphics::surfacecpy(surface, &closeButtonSurface, {pos.x + info.width - 21, pos.y + 3});

	Lemon::Graphics::DrawString(info.title, pos.x + 6, pos.y + 6, 255, 255, 255, surface);

	vector2i_t renderPos = pos + (vector2i_t){1, WINDOW_TITLEBAR_HEIGHT + 1};
	syscall(SYS_DESKTOP_GET_WINDOW, &info, info.handle, 0, 0, 0);
	surface_t wSurface = {.width = info.width, .height = info.height, .buffer = (info.currentBuffer ? secondaryBuffer : primaryBuffer)};
	Lemon::Graphics::surfacecpy(surface, &wSurface, renderPos);
}

rect_t Window::GetRect(){
    rect_t rect = {pos, {info.width, info.height}};

    if(!(info.flags & WINDOW_FLAGS_NODECORATION)){
        rect.size += (vector2i_t){2, WINDOW_TITLEBAR_HEIGHT + 2};
    }

    return rect;
}