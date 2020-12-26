#include "lemonwm.h"

#include <lemon/gfx/graphics.h>
#include <lemon/gui/window.h>
#include <lemon/core/sharedmem.h>
#include <stdlib.h>

WMWindow::WMWindow(WMInstance* wm, handle_t endp, handle_t id, int64_t key, WindowBuffer* bufferInfo, vector2i_t pos, vector2i_t size, unsigned int flags, const std::string& title) : Endpoint(endp){
	this->wm = wm;

	clientID = id;
	
	bufferKey = key;
    windowBufferInfo = bufferInfo;

    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

	this->pos = pos;
	this->size = size;
	this->flags = flags;
	this->title = title;
	
	Queue(Lemon::Message(Lemon::GUI::WindowBufferReturn, bufferKey));
}

WMWindow::~WMWindow(){
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
	Lemon::Graphics::DrawGradientVertical({pos + (vector2i_t){1,1}, {size.x + WINDOW_BORDER_THICKNESS, WINDOW_TITLEBAR_HEIGHT}}, {0x33, 0x2c, 0x29, 255}, {0x2e, 0x29, 0x29, 255}, surface);

	Lemon::Graphics::DrawString(title.c_str(), pos.x + 6, pos.y + 6, 255, 255, 255, surface);

	surface_t* buttons = &wm->compositor.windowButtons;

	if(Lemon::Graphics::PointInRect({{closeRect.x + pos.x, closeRect.y + pos.y}, closeRect.size}, wm->input.mouse.pos)){
		Lemon::Graphics::surfacecpy(surface, buttons, pos + closeRect.pos, {{0, 19}, {19, 19}}); // Close button
	} else {
		Lemon::Graphics::surfacecpyTransparent(surface, buttons, pos + closeRect.pos, {{0, 0}, {19, 19}}); // Close button
	}

	if(Lemon::Graphics::PointInRect({{pos.x + minimizeRect.x, pos.y + minimizeRect.y}, minimizeRect.size}, wm->input.mouse.pos)){
		Lemon::Graphics::surfacecpy(surface, buttons, pos + minimizeRect.pos, {{19, 19}, {19, 19}}); // Minimize button
	} else {
		Lemon::Graphics::surfacecpyTransparent(surface, buttons, pos + minimizeRect.pos, {{19, 0}, {19, 19}}); // Minimize button
	}

	windowBufferInfo->drawing = 1;
    surface_t wSurface = {.width = size.x, .height = size.y, .buffer = ((windowBufferInfo->currentBuffer == 0) ? buffer1 : buffer2)};
	
	vector2i_t clipOffset =  pos + (vector2i_t){WINDOW_BORDER_THICKNESS, WINDOW_BORDER_THICKNESS + WINDOW_TITLEBAR_HEIGHT};
	
	#ifdef LEMONWM_USE_CLIPPING
		for(rect_t& clip : clips){
			Lemon::Graphics::surfacecpy(surface, &wSurface, clip.pos, {clip.pos - clipOffset, clip.size});
		}
	#else
		Lemon::Graphics::surfacecpy(surface, &wSurface, clipOffset);
	#endif

	windowBufferInfo->drawing = 0;
}

void WMWindow::Minimize(bool state){
	minimized = state;
}

void WMWindow::Resize(vector2i_t size, int64_t key, WindowBuffer* bufferInfo){
	Lemon::UnmapSharedMemory(windowBufferInfo, bufferKey);

	bufferKey = key;

    windowBufferInfo = bufferInfo;

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