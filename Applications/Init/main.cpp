#include <stdint.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <gfx/graphics.h>
#include <lemon/filesystem.h>
#include <gfx/window/window.h>
#include <stdio.h>
#include <list.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
#include <lemon/itoa.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "clip.h"

#define WINDOW_BORDER_COLOUR {32,32,32}
#define WINDOW_TITLEBAR_HEIGHT 24

#define c0 {0, 0, 0, 0}
#define c1 {255, 255, 255, 255}
#define c2 {0, 0, 0, 255}
#define c3 {33, 33, 33, 255}
#define c4 {66, 66, 66, 255}

rgba_colour_t mouse[] = {
	c2,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c2,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c2,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c2,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c2,c0,c0,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c2,c0,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c2,c0,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c2,c0,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c2,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c1,c2,c0,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c1,c1,c2,c0,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c1,c1,c1,c2,c0,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c1,c1,c1,c1,c2,c0,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c1,c1,c1,c1,c1,c2,c0,
	c2,c1,c1,c1,c1,c1,c1,c1,c3,c3,c3,c3,c3,c3,c2,
	c2,c1,c1,c1,c1,c3,c1,c1,c2,c2,c2,c2,c2,c2,c2,
	c2,c1,c1,c1,c3,c2,c3,c1,c2,c0,c0,c0,c0,c0,c0,
	c2,c1,c1,c3,c2,c0,c2,c4,c1,c2,c0,c0,c0,c0,c0,
	c2,c1,c3,c2,c0,c0,c2,c3,c1,c2,c0,c0,c0,c0,c0,
	c2,c3,c2,c0,c0,c0,c0,c2,c4,c1,c2,c0,c0,c0,c0,
	c2,c2,c0,c0,c0,c0,c0,c2,c3,c1,c2,c0,c0,c0,c0,
	c2,c0,c0,c0,c0,c0,c0,c0,c2,c4,c1,c2,c0,c0,c0,
	c0,c0,c0,c0,c0,c0,c0,c0,c2,c3,c1,c2,c0,c0,c0,
	c0,c0,c0,c0,c0,c0,c0,c0,c0,c2,c2,c0,c0,c0,c0,
};

surface_t mouseSurface = {
	.x = 0,
	.y = 0,
	.width = 15,
	.height = 24,
	.buffer = (uint8_t*)mouse,
};

int keymap_us[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	KEY_CONTROL,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   KEY_SHIFT,		/* Left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
	KEY_ALT,	/* Alt */
	' ',	/* Space bar */
	0,	/* Caps lock */
	KEY_F1,	/* 59 - F1 key ... > */
	KEY_F2,   KEY_F3,   KEY_F4,   KEY_F5,   KEY_F6,   KEY_F7,   KEY_F8,   KEY_F9,
	KEY_F10,	/* < ... F10 */
	0,	/* 69 - Num lock*/
	0,	/* Scroll Lock */
	0,	/* Home key */
	KEY_ARROW_UP,	/* Up Arrow */
	0,	/* Page Up */
	'-',
	KEY_ARROW_LEFT,	/* Left Arrow */
	0,
	KEY_ARROW_RIGHT,	/* Right Arrow */
	'+',
	0,	/* 79 - End key*/
	KEY_ARROW_DOWN,	/* Down Arrow */
	0,	/* Page Down */
	0,	/* Insert Key */
	0,	/* Delete Key */
	0,   0,   0,
	0,	/* F11 Key */
	0,	/* F12 Key */
	0,	/* All other keys are undefined */
};

struct Window_s{
	handle_t handle;
	win_info_t info;
	vector2i_t pos;
	bool active = false;
};

struct KeyboardState{
	bool caps, control, shift, alt;
};

KeyboardState keyboardState;

uint8_t* fb = 0;
fb_info_t fbInfo;
surface_t renderBuffer;

int8_t mouseData[3];
vector2i_t mousePos = {100,100};
bool mouseDown = false;
bool drag = false;
vector2i_t dragOffset = {0,0};

bool redrawWindowDecorations = true;

char lastKey;

//#define ENABLE_BACKGROUND_IMAGE

#define ENABLE_FRAMERATE_COUNTER
#ifdef ENABLE_FRAMERATE_COUNTER

size_t frameCounter;

uint64_t lastUptimeSeconds;
uint64_t lastUptimeMilliseconds;

int frameRate;

uint64_t currentUptimeSeconds;
uint64_t currentUptimeMilliseconds;

void UpdateFrameRate(){
	syscall(SYS_UPTIME, (uintptr_t)&currentUptimeSeconds, (uintptr_t)&currentUptimeMilliseconds, 0, 0, 0);
	uint64_t difference = currentUptimeSeconds*1000 - lastUptimeSeconds*1000 - lastUptimeMilliseconds + currentUptimeMilliseconds;
	if(difference >= 2000){
		frameRate = frameCounter/(difference/1000);
		frameCounter = 0;
		lastUptimeSeconds = currentUptimeSeconds;
		lastUptimeMilliseconds = currentUptimeMilliseconds;
	}
}
#endif

rgba_colour_t backgroundColor = {64, 128, 128};
surface_t closeButtonSurface;
surface_t backgroundImageSurface;
	
List<rect_t> clipRects;

int windowCount; // Window Count
List<Window_s*> windows;
Window_s* active; // Active Window

void AddClip(rect_t rect){
retry:
	for(int i = 0; i < clipRects.get_length(); i++){
		rect_t clip = clipRects[i];
		if(!(GET_LEFT(clip) <= GET_RIGHT(rect) && GET_RIGHT(clip) >= GET_LEFT(rect) && GET_TOP(clip) <= GET_BOTTOM(rect) && GET_BOTTOM(clip) >= GET_TOP(rect)))
		{	
			continue;
		}

		SplitRect(rect, clip, &clipRects);
		clipRects.remove_at(i);
		goto retry;
	}

	clipRects.add_back(rect);
}

void RemoveDestroyedWindows(){
	int _windowCount; // Updated Window Count
	syscall(SYS_DESKTOP_GET_WINDOW_COUNT, (uintptr_t)(&_windowCount),0,0,0,0);
	windowCount = _windowCount;
	for(int i = 0; i < windows.get_length(); i++){ // Remove any windows that no longer exist
			bool windowFound = false;
			for(int j = 0; j < windowCount; j++){
				win_info_t windowInfo;
				handle_t windowHandle;

				syscall(SYS_DESKTOP_GET_WINDOW,(uintptr_t)&windowInfo,j,0,0,0);
				windowHandle = windowInfo.handle;

				if(windows[i]->handle == windowHandle){
					windowFound = true;
					break;
				}
			}

			if(!windowFound) {
				Window_s* w = windows.remove_at(i);
				delete w;
				redrawWindowDecorations = true;
			}
		}
}

void AddNewWindows(){
	int _windowCount; // Updated Window Count
	syscall(SYS_DESKTOP_GET_WINDOW_COUNT, (uintptr_t)(&_windowCount),0,0,0,0);
	windowCount = _windowCount;
	
	for(int j = 0; j < windowCount; j++){ // Loop through windows and match with what the kernel has
		bool windowFound = false;
		win_info_t windowInfo;
		handle_t windowHandle;

		syscall(SYS_DESKTOP_GET_WINDOW,(uintptr_t)&windowInfo,j,0,0,0);
		windowHandle = windowInfo.handle;

		for(int i = 0; i < windows.get_length(); i++){
			if(windows[i]->handle == windowHandle){ // We already know about this window
				windows[i]->info = windowInfo;
				windowFound = true;
				break;
			}
		}

		if(!windowFound){ // New window?
			Window_s* win = (Window_s*)malloc(sizeof(Window_s));
			win->info = windowInfo;
			win->handle = windowHandle;
			win->pos = {win->info.x, win->info.y};
			windows.add_back(win);
		}
	}
	
	redrawWindowDecorations = true;
}

bool PointInWindow(Window_s* win, vector2i_t point){
	int windowHeight = (win->info.flags & WINDOW_FLAGS_NODECORATION) ? win->info.height : (win->info.height + 25); // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos},{win->info.width, windowHeight}}, point);
}

bool PointInWindowProper(Window_s* win, vector2i_t point){
	int windowYOffset = (win->info.flags & WINDOW_FLAGS_NODECORATION) ? 0 : 25; // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos + (vector2i_t){0, windowYOffset}},{win->info.width, win->info.height}}, point);
}

void DrawWindow(Window_s* win){
	if(win->info.flags & WINDOW_FLAGS_MINIMIZED){
		return;
	}

	if(win->info.flags & WINDOW_FLAGS_NODECORATION){
		syscall(SYS_RENDER_WINDOW, (uintptr_t)&renderBuffer, (uintptr_t)win->handle,(uintptr_t)&win->pos,0,0);
		return;
	}

	//if(redrawWindowDecorations){
		Lemon::Graphics::DrawRect({win->pos + (vector2i_t){1,0}, {win->info.width,1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Top Border
		Lemon::Graphics::DrawRect({win->pos + (vector2i_t){0,1}, {1, win->info.height + WINDOW_TITLEBAR_HEIGHT}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Left border
		Lemon::Graphics::DrawRect({win->pos + (vector2i_t){0, win->info.height + WINDOW_TITLEBAR_HEIGHT + 1}, {win->info.width + 1, 1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Bottom border
		Lemon::Graphics::DrawRect({win->pos + (vector2i_t){win->info.width + 1, 1}, {1,win->info.height + WINDOW_TITLEBAR_HEIGHT + 1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Right border

		Lemon::Graphics::DrawGradientVertical({win->pos + (vector2i_t){1,1}, {win->info.width, WINDOW_TITLEBAR_HEIGHT}}, {96, 96, 96}, {42, 50, 64}, &renderBuffer);

		Lemon::Graphics::surfacecpy(&renderBuffer, &closeButtonSurface, {win->pos.x + win->info.width - 21, win->pos.y + 3});

		Lemon::Graphics::DrawString(win->info.title, win->pos.x + 6, win->pos.y + 6, 255, 255, 255, &renderBuffer);
	//}

	vector2i_t renderPos = win->pos + (vector2i_t){1, WINDOW_TITLEBAR_HEIGHT + 1};
	syscall(SYS_RENDER_WINDOW, (uintptr_t)&renderBuffer, (uintptr_t)win->handle,(uintptr_t)&renderPos,0,0);
}

void memcpy_optimized(void* dest, void* src, size_t count);
int main(){
	fb = lemon_map_fb(&fbInfo); // Request framebuffer mapping from kernel
	surface_t* fbSurface = Lemon::Graphics::CreateFramebufferSurface(fbInfo,fb); // Create surface object for framebuffer
	renderBuffer = *fbSurface;
	renderBuffer.buffer = (uint8_t*)malloc(renderBuffer.pitch * renderBuffer.height); // Render buffer/Double buffer

	Lemon::Graphics::DrawRect(0,0,fbSurface->width,fbSurface->height,255,0,128, fbSurface);

	syscall(SYS_CREATE_DESKTOP,0,0,0,0,0); // Get Kernel to create Desktop

	FILE* closeButtonFile = fopen("/initrd/close.bmp", "r");

	if(!closeButtonFile) {
		return 1;
	}

	fseek(closeButtonFile, 0, SEEK_END);
	uint64_t closeButtonLength = ftell(closeButtonFile);
	fseek(closeButtonFile, 0, SEEK_SET);

	uint8_t* closeButtonBuffer = (uint8_t*)malloc(closeButtonLength + (closeButtonLength));
	fread(closeButtonBuffer, closeButtonLength, 1, closeButtonFile);
	bitmap_info_header_t* closeInfoHeader = ((bitmap_info_header_t*)(closeButtonBuffer + sizeof(bitmap_file_header_t)));
	closeButtonSurface.width = closeInfoHeader->width;
	closeButtonSurface.height = closeInfoHeader->height;
	closeButtonSurface.buffer = (uint8_t*)malloc(19*19*4);
	Lemon::Graphics::DrawBitmapImage(0, 0, closeButtonSurface.width, closeButtonSurface.height, closeButtonBuffer, &closeButtonSurface);
	free(closeButtonBuffer);

	#ifdef ENABLE_BACKGROUND_IMAGE
	backgroundImageSurface.width = renderBuffer.width;
	backgroundImageSurface.height = renderBuffer.height;
	backgroundImageSurface.buffer = (uint8_t*)malloc(backgroundImageSurface.width * backgroundImageSurface.height * 4);
	int e = Lemon::Graphics::LoadImage("/initrd/bg2.png", 0, 0, backgroundImageSurface.width, backgroundImageSurface.height, &backgroundImageSurface, true);
	syscall(0, "e: ", e, 0, 0, 0);
	#endif

	syscall(SYS_EXEC, (uintptr_t)"/initrd/shell.lef", 0, 0, 0, 0);

	int mouseDevice = lemon_open("/dev/mouse0", 0);
	lemon_read(mouseDevice, mouseData, 3);

	Lemon::Graphics::DrawRect(0, 0, renderBuffer.width, renderBuffer.height, backgroundColor, &renderBuffer);

	for(;;){
		#ifdef ENABLE_FRAMERATE_COUNTER
		frameCounter++;
		UpdateFrameRate();
		#endif

		if(redrawWindowDecorations){
			#ifdef ENABLE_BACKGROUND_IMAGE
			Lemon::Graphics::surfacecpy(&renderBuffer, &backgroundImageSurface);
			#else
			Lemon::Graphics::DrawRect(0, 0, renderBuffer.width, renderBuffer.height, backgroundColor, &renderBuffer);
			#endif
		}
		
		for(int i = 0; i < windowCount; i++){ // Draw all our windows
			Window_s* win = windows[i];
			DrawWindow(win);
		}
		redrawWindowDecorations = false;

		lemon_read(mouseDevice, mouseData, 3);
		
		mousePos.x += mouseData[1] * 2;
		mousePos.y -= mouseData[2] * 2;

		if(mousePos.x < 0) mousePos.x = 0;
		if(mousePos.y < 0) mousePos.y = 0;

		if(mousePos.x >= renderBuffer.width) mousePos.x = renderBuffer.width;
		if(mousePos.y >= renderBuffer.height) mousePos.y = renderBuffer.height;

		if(drag){
			redrawWindowDecorations = true;
			active->pos.x = mousePos.x - dragOffset.x;
			active->pos.y = mousePos.y - dragOffset.y;
			if(active->pos.x < 0) active->pos.x = 0;
			if(active->pos.y < 0) active->pos.y = 0;
		}

		if((mouseData[0] & 0x1) && !mouseDown){
			mouseDown = true;
			for(int i = windows.get_length()-1; i >= 0; i--){
				Window_s* win = windows[i];
				if(PointInWindow(win, mousePos)){
					windows.add_back(windows.remove_at(i));
					active = win;
					redrawWindowDecorations = true;
					if(mousePos.y < win->pos.y + 25 && !(win->info.flags & WINDOW_FLAGS_NODECORATION)){
						if(mousePos.x > win->pos.x + win->info.width - 21 && mousePos.y > win->pos.y + 3 && mousePos.x < win->pos.x + win->info.width - (21 - 19)){
							ipc_message_t closeMsg;
							closeMsg.msg = WINDOW_EVENT_CLOSE;
							Lemon::SendMessage(win->info.ownerPID, closeMsg);
						} else {
							dragOffset = {mousePos.x - win->pos.x, mousePos.y - win->pos.y};
							drag = true;
						}
					} else {
						ipc_message_t mouseEventMessage;
						mouseEventMessage.msg = WINDOW_EVENT_MOUSEDOWN;
						uint32_t mouseX;
						uint32_t mouseY;
						if(win->info.flags & WINDOW_FLAGS_NODECORATION){
							mouseX = mousePos.x - win->pos.x;
							mouseY = mousePos.y - win->pos.y;
						} else {
							mouseX = mousePos.x - win->pos.x - 1; // Account for window border
							mouseY = mousePos.y - win->pos.y - 25; // Account for border and title bar
						}
						mouseEventMessage.data = (((uint64_t)mouseX) << 32) | mouseY;
						mouseEventMessage.data2 = (uintptr_t)win->info.handle;
						Lemon::SendMessage(win->info.ownerPID, mouseEventMessage);
					}
					break;
				}
			}
		}

		if (!(mouseDown) && drag){
			drag = false;
		}

		if(mouseDown && !(mouseData[0] & 0x1)){
			mouseDown = false;
			drag = false;
			if(active && PointInWindowProper(active, mousePos)){
				redrawWindowDecorations = true;
				if(active) {
					ipc_message_t mouseEventMessage;
					mouseEventMessage.msg = WINDOW_EVENT_MOUSEUP;
					uint32_t mouseX;
					uint32_t mouseY;
					if(active->info.flags & WINDOW_FLAGS_NODECORATION){
						mouseX = mousePos.x - active->pos.x;
						mouseY = mousePos.y - active->pos.y;
					} else {
						mouseX = mousePos.x - active->pos.x - 1; // Account for window border
						mouseY = mousePos.y - active->pos.y - 25; // Account for border and title bar
					}
					mouseEventMessage.data = (((uint64_t)mouseX) << 32) | mouseY;
					mouseEventMessage.data2 = (uintptr_t)active->info.handle;
					Lemon::SendMessage(active->info.ownerPID, mouseEventMessage);
				}
			}
		}

		if(active && PointInWindowProper(active, mousePos)){
			ipc_message_t mouseEventMessage;
			mouseEventMessage.msg = WINDOW_EVENT_MOUSEMOVE;
			uint32_t mouseX;
			uint32_t mouseY;
			if(active->info.flags & WINDOW_FLAGS_NODECORATION){
				mouseX = mousePos.x - active->pos.x;
				mouseY = mousePos.y - active->pos.y;
			} else {
				mouseX = mousePos.x - active->pos.x - 1; // Account for window border
				mouseY = mousePos.y - active->pos.y - 25; // Account for border and title bar
			}
			mouseEventMessage.data = (((uint64_t)mouseX) << 32) | mouseY;
			mouseEventMessage.data2 = (uintptr_t)active->info.handle;
			Lemon::SendMessage(active->info.ownerPID, mouseEventMessage);
		}
		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			switch(msg.msg){
				case DESKTOP_EVENT_KEY:
					if(active && (msg.data & 0x7F) < 128 /*Values above 128 would exceed the length of the keymap*/){
						ipc_message_t keyMsg;

						int key = keymap_us[msg.data & 0x7F];

						switch(key){
							case KEY_SHIFT:
								keyboardState.shift = !(msg.data >> 7);
								break;
							case KEY_CONTROL:
								keyboardState.control = !(msg.data >> 7);
								break;
							case KEY_ALT:
								keyboardState.alt = !(msg.data >> 7);
								break;
						}

						keyMsg.data2 = 0;

						if(keyboardState.shift) keyMsg.data2 |= KEY_STATE_SHIFT;
						if(keyboardState.control) keyMsg.data2 |= KEY_STATE_CONTROL;
						if(keyboardState.alt) keyMsg.data2 |= KEY_STATE_ALT;

						if((/*msg.data2 /*caps* / ||*/ keyboardState.shift) && isalpha(key)) key = toupper(key);

						if((msg.data >> 7) && (msg.data & 0x7F)) {
							keyMsg.msg = WINDOW_EVENT_KEYRELEASED;
						} else {
							keyMsg.msg = WINDOW_EVENT_KEY;
						}
						keyMsg.data = key;
						keyMsg.data2 = (uintptr_t)active->info.handle;
						Lemon::SendMessage(active->info.ownerPID, keyMsg);
					}

					break;
				case DESKTOP_EVENT:
					if(msg.data == DESKTOP_SUBEVENT_WINDOW_DESTROYED){
						RemoveDestroyedWindows();
					} else if (msg.data == DESKTOP_SUBEVENT_WINDOW_CREATED){
						AddNewWindows();
					}
					break;
			}
		}

		/*clipRects.clear();
		for(int i = windows.get_length() - 1; i >= 0; i--){
			Window_s* win = windows[i];
			rect_t rect = {win->pos, {win->info.width, win->info.height}};

			AddClip(rect);
		}

		for(int i = 0; i < clipRects.get_length(); i++){
			rect_t rect = clipRects[i];

			if(rect.pos.x <= 0 && rect.pos.y <= 0 && rect.size.x <= 0 && rect.size.y <= 0) continue;

			Lemon::Graphics::DrawRect(rect.pos.x, rect.pos.y, 1, rect.size.y, 255, 0, 0, &renderBuffer);
			Lemon::Graphics::DrawRect(rect.pos.x, rect.pos.y, rect.size.x, 1, 255, 0, 0, &renderBuffer);
			Lemon::Graphics::DrawRect(rect.pos.x + rect.size.x, rect.pos.y, 1, rect.size.y, 255, 0, 0, &renderBuffer);
			Lemon::Graphics::DrawRect(rect.pos.x, rect.pos.y + rect.size.y, rect.size.x, 1, 255, 0, 0, &renderBuffer);
		}*/

		#ifdef ENABLE_FRAMERATE_COUNTER
		Lemon::Graphics::DrawRect(0,0,8*3,12,0,0,0,&renderBuffer);
		char temp[5];
		memset(temp,5,0);
		itoa(frameRate, temp, 10);
		Lemon::Graphics::DrawString(temp,0,0,255,255,255,&renderBuffer);
		#endif

		Lemon::Graphics::surfacecpyTransparent(&renderBuffer, &mouseSurface, {mousePos.x, mousePos.y});
		
		memcpy_optimized(fbSurface->buffer, renderBuffer.buffer, fbInfo.width * fbInfo.height << 2); // Render our buffer

		#ifdef ENABLE_BACKGROUND_IMAGE
		Lemon::Graphics::surfacecpy(&renderBuffer, &backgroundImageSurface, mousePos, {mousePos, {mouseSurface.width, mouseSurface.height}});
		#else
		Lemon::Graphics::DrawRect(mousePos.x, mousePos.y, mouseSurface.width, mouseSurface.height, backgroundColor, &renderBuffer);
		#endif
	}

	for(;;);
}
