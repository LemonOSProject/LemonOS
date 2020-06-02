#include "lemonwm.h"

#include <gfx/graphics.h>
#include <gui/window.h>
#include <core/sharedmem.h>

WMWindow::WMWindow(unsigned long key){
    windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(key);
    windowBufferInfo->buffer1Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F));
    windowBufferInfo->buffer2Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((size.x * size.y * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes*/);

    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;
}

void WMWindow::Draw(surface_t* surface){
	/*if(info.flags & WINDOW_FLAGS_MINIMIZED){
		return;
	}*/

	if(flags & WINDOW_FLAGS_NODECORATION){
		surface_t wSurface = {.width = size.x, .height = size.y, .buffer = ((windowBufferInfo->currentBuffer == 0) ? buffer1 : buffer2)};
		Lemon::Graphics::surfacecpy(surface, &wSurface, pos);
		return;
	}

	Lemon::Graphics::DrawRectOutline(pos.x, pos.y, size.x + 2, size.y + WINDOW_TITLEBAR_HEIGHT + 2, WINDOW_BORDER_COLOUR, surface);
	Lemon::Graphics::DrawGradientVertical({pos + (vector2i_t){1,1}, {size.x, WINDOW_TITLEBAR_HEIGHT}}, {96, 96, 96}, {42, 50, 64}, surface);

	//Lemon::Graphics::surfacecpy(surface, &closeButtonSurface, {pos.x + size.x - 21, pos.y + 3});

	Lemon::Graphics::DrawString(title, pos.x + 6, pos.y + 6, 255, 255, 255, surface);

	vector2i_t renderPos = pos + (vector2i_t){1, WINDOW_TITLEBAR_HEIGHT + 1};
    surface_t wSurface = {.width = size.x, .height = size.y, .buffer = ((windowBufferInfo->currentBuffer == 0) ? buffer1 : buffer2)};
    Lemon::Graphics::surfacecpy(surface, &wSurface, renderPos);
}