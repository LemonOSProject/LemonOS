#include "lemonwm.h"

#include <gfx/graphics.h>
#include <gui/window.h>
#include <core/sharedmem.h>
#include <stdlib.h>

WMWindow::WMWindow(WMInstance* wm, unsigned long key){
	this->wm = wm;
	bufferKey = key;

    windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(key);

    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;
}

WMWindow::~WMWindow(){
	free(title);

	Lemon::UnmapSharedMemory(windowBufferInfo, bufferKey);
}

void WMWindow::Draw(surface_t* surface){
	if(minimized) return;

	if(flags & WINDOW_FLAGS_NODECORATION){
		surface_t wSurface = {.width = size.x, .height = size.y, .buffer = ((windowBufferInfo->currentBuffer == 0) ? buffer1 : buffer2)};
		Lemon::Graphics::surfacecpy(surface, &wSurface, pos);
		return;
	}

	Lemon::Graphics::DrawRectOutline(pos.x, pos.y, size.x + WINDOW_BORDER_THICKNESS * 2, size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 2, WINDOW_BORDER_COLOUR, surface);
	Lemon::Graphics::DrawRectOutline(pos.x + (WINDOW_BORDER_THICKNESS / 2), pos.y + WINDOW_TITLEBAR_HEIGHT + (WINDOW_BORDER_THICKNESS / 2), size.x + WINDOW_BORDER_THICKNESS, size.y + WINDOW_BORDER_THICKNESS, {42, 50, 64}, surface);
	Lemon::Graphics::DrawGradientVertical({pos + (vector2i_t){1,1}, {size.x + WINDOW_BORDER_THICKNESS, WINDOW_TITLEBAR_HEIGHT}}, {96, 96, 96}, {42, 50, 64}, surface);

	Lemon::Graphics::DrawString(title, pos.x + 6, pos.y + 6, 255, 255, 255, surface);

	windowBufferInfo->drawing = 1;
	vector2i_t renderPos = pos + (vector2i_t){WINDOW_BORDER_THICKNESS, WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS};
    surface_t wSurface = {.width = size.x, .height = size.y, .buffer = ((windowBufferInfo->currentBuffer == 0) ? buffer1 : buffer2)};
    Lemon::Graphics::surfacecpy(surface, &wSurface, renderPos);
	windowBufferInfo->drawing = 0;

	surface_t* buttons = &wm->compositor.windowButtons;
	Lemon::Graphics::surfacecpy(surface, buttons, pos + closeRect.pos, {{0, 0}, {19, 19}}); // Close button
	Lemon::Graphics::surfacecpy(surface, buttons, pos + minimizeRect.pos, {{19, 0},{19, 19}}); // Minimize button
}

void WMWindow::Minimize(bool state){
	minimized = state;
}

void WMWindow::Resize(vector2i_t size, unsigned long key){
	Lemon::UnmapSharedMemory(windowBufferInfo, bufferKey);

	bufferKey = key;

    windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(key);

    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

	this->size = size;

	RecalculateButtonRects();
}

rect_t WMWindow::GetCloseRect(){
	rect_t r = closeRect;
	r.pos += pos;
	return r;
}

rect_t WMWindow::GetMinimizeRect(){
	rect_t r = minimizeRect;
	r.pos += pos;
	return r;
}

void WMWindow::RecalculateButtonRects(){
	surface_t* buttons = &wm->compositor.windowButtons;
	closeRect = {{size.x - (buttons->width / 3) - 2, (12 - ((buttons->height / 3) / 2))}, {buttons->width / 3, buttons->height / 3}};
	minimizeRect = {{size.x - (buttons->width / 3) * 2 - 4, (12 - ((buttons->height / 3) / 2))}, {buttons->width / 3, buttons->height / 3}};
}

rect_t WMWindow::GetBottomBorderRect(){ // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x - WINDOW_BORDER_THICKNESS, pos.y + size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS}, {size.x + WINDOW_BORDER_THICKNESS * 3, WINDOW_BORDER_THICKNESS * 2}};
}

rect_t WMWindow::GetTopBorderRect(){ // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x - WINDOW_BORDER_THICKNESS, pos.y - WINDOW_BORDER_THICKNESS /*Extend the border rect out a bit*/}, {size.x + WINDOW_BORDER_THICKNESS * 3, WINDOW_BORDER_THICKNESS * 2}};
}

rect_t WMWindow::GetLeftBorderRect(){ // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x - WINDOW_BORDER_THICKNESS, pos.y - WINDOW_BORDER_THICKNESS}, {WINDOW_BORDER_THICKNESS * 2, size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 3}};
}

rect_t WMWindow::GetRightBorderRect(){ // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x + size.x + WINDOW_BORDER_THICKNESS, pos.y - WINDOW_BORDER_THICKNESS}, {WINDOW_BORDER_THICKNESS * 2, size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 3}};
}