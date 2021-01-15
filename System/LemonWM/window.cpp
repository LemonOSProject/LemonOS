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

	windowSurface = { .width = size.x, .height = size.y, .depth = 32, .buffer = buffer1 };
	
	Queue(Lemon::Message(Lemon::GUI::WindowBufferReturn, bufferKey));
}

WMWindow::~WMWindow(){
	Lemon::UnmapSharedMemory(windowBufferInfo, bufferKey);
}

void WMWindow::DrawDecoration(surface_t* surface, rect_t clip) const{
	if(flags & WINDOW_FLAGS_NODECORATION){
		return;
	}

	Lemon::Graphics::DrawRectOutline(pos.x, pos.y, size.x + WINDOW_BORDER_THICKNESS * 2, size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 2, WINDOW_BORDER_COLOUR, surface, clip);
	Lemon::Graphics::DrawRectOutline(pos.x + (WINDOW_BORDER_THICKNESS / 2), pos.y + WINDOW_TITLEBAR_HEIGHT + (WINDOW_BORDER_THICKNESS / 2), size.x + WINDOW_BORDER_THICKNESS, size.y + WINDOW_BORDER_THICKNESS, {42, 50, 64}, surface, clip);
	Lemon::Graphics::DrawGradientVertical({pos + (vector2i_t){1,1}, {size.x + WINDOW_BORDER_THICKNESS, WINDOW_TITLEBAR_HEIGHT}}, {0x33, 0x2c, 0x29, 255}, {0x2e, 0x29, 0x29, 255}, surface, clip);

	Lemon::Graphics::DrawString(title.c_str(), pos.x + 6, pos.y + 6, 255, 255, 255, surface, clip);

	const surface_t* buttons = &wm->compositor.windowButtons;

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
}

void WMWindow::DrawClip(surface_t* surface, rect_t clip){
	windowSurface.buffer = (windowBufferInfo->currentBuffer == 0) ? buffer1 : buffer2;

	if(!(flags & WINDOW_FLAGS_NODECORATION)){
		// Ensure that the clip rect is only within bounds of the render surface
		if(clip.top() < pos.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS || clip.left() <= pos.x + WINDOW_BORDER_THICKNESS || clip.right() >= pos.x + size.x + WINDOW_BORDER_THICKNESS || clip.bottom() >= pos.y + size.y + WINDOW_BORDER_THICKNESS + WINDOW_TITLEBAR_HEIGHT){
			DrawDecoration(surface, clip);
		}

		clip.top(std::max(clip.top(), pos.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS));
		if(clip.height <= 0) return;

		clip.left(std::max(clip.left(), pos.x + WINDOW_BORDER_THICKNESS));
		if(clip.width <= 0) return;

		Lemon::Graphics::surfacecpy(surface, &windowSurface, clip.pos, {clip.pos - pos - (vector2i_t){WINDOW_BORDER_THICKNESS, WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS}, clip.size});
	} else {
		Lemon::Graphics::surfacecpy(surface, &windowSurface, clip.pos, {clip.pos - pos, clip.size});
	}
}

/*void WMWindow::Draw(surface_t* surface){
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
	
	Lemon::Graphics::surfacecpy(surface, &wSurface, clipOffset);

	windowBufferInfo->drawing = 0;
	windowBufferInfo->dirty = 1;
}*/

void WMWindow::Minimize(bool state){
	minimized = state;

	if(!minimized){
		windowBufferInfo->dirty = true;
	}
}

void WMWindow::Resize(vector2i_t size, int64_t key, WindowBuffer* bufferInfo){
	Lemon::UnmapSharedMemory(windowBufferInfo, bufferKey);

	bufferKey = key;

    windowBufferInfo = bufferInfo;

    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

	this->size = size;

	windowSurface = { .width = size.x, .height = size.y, .depth = 32, .buffer = buffer1 };

	RecalculateButtonRects();

	Queue(Lemon::Message(Lemon::GUI::WindowBufferReturn, bufferKey));
}

rect_t WMWindow::GetWindowRect() const {
	rect_t r = { pos, size };

	if(!(flags & WINDOW_FLAGS_NODECORATION)){ // Account for all four borders and the titlebar
		r.size += { WINDOW_BORDER_THICKNESS * 2, WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 2 };
	}

	return r;
}

rect_t WMWindow::GetCloseRect() const {
	rect_t r = closeRect;
	r.pos += pos;
	return r;
}

rect_t WMWindow::GetMinimizeRect() const {
	rect_t r = minimizeRect;
	r.pos += pos;
	return r;
}

void WMWindow::RecalculateButtonRects(){
	surface_t* buttons = &wm->compositor.windowButtons;
	closeRect = {{size.x - (buttons->width / 3) - 2, (12 - ((buttons->height / 3) / 2))}, {buttons->width / 3, buttons->height / 3}};
	minimizeRect = {{size.x - (buttons->width / 3) * 2 - 4, (12 - ((buttons->height / 3) / 2))}, {buttons->width / 3, buttons->height / 3}};
}

rect_t WMWindow::GetBottomBorderRect() const { // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x - WINDOW_BORDER_THICKNESS, pos.y + size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS}, {size.x + WINDOW_BORDER_THICKNESS * 3, WINDOW_BORDER_THICKNESS * 2}};
}

rect_t WMWindow::GetTopBorderRect() const { // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x - WINDOW_BORDER_THICKNESS, pos.y - WINDOW_BORDER_THICKNESS /*Extend the border rect out a bit*/}, {size.x + WINDOW_BORDER_THICKNESS * 3, WINDOW_BORDER_THICKNESS * 2}};
}

rect_t WMWindow::GetLeftBorderRect() const { // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x - WINDOW_BORDER_THICKNESS, pos.y - WINDOW_BORDER_THICKNESS}, {WINDOW_BORDER_THICKNESS * 2, size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 3}};
}

rect_t WMWindow::GetRightBorderRect() const { // Windows with no titlebar also have no borders so don't worry about checking for the no decoration flag
	return {{pos.x + size.x + WINDOW_BORDER_THICKNESS, pos.y - WINDOW_BORDER_THICKNESS}, {WINDOW_BORDER_THICKNESS * 2, size.y + WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS * 3}};
}