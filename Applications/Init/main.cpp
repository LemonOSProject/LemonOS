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

#define WINDOW_BORDER_COLOUR {32,32,32}
#define WINDOW_TITLEBAR_HEIGHT 24

int keymap_us[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	0,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   0,		/* Left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
	0,	/* Alt */
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

uint8_t* fb = 0;

fb_info_t fbInfo;

surface_t renderBuffer;

int8_t mouseData[3];
vector2i_t mousePos = {100,100};
bool mouseDown = false;
bool drag = false;
vector2i_t dragOffset = {0,0};

void DrawWindow(Window_s* win){

	if(win->info.flags & WINDOW_FLAGS_NODECORATION){
		syscall(SYS_RENDER_WINDOW, (uintptr_t)&renderBuffer, (uintptr_t)win->handle,(uintptr_t)&win->pos,0,0);
		return;
	}

	DrawRect({win->pos, {win->info.width + 1,1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Top Border
	DrawRect({win->pos, {1, win->info.height + WINDOW_TITLEBAR_HEIGHT + 1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Left border
	DrawRect({win->pos + (vector2i_t){0, win->info.height + WINDOW_TITLEBAR_HEIGHT + 1}, {win->info.width + 1, 1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Bottom border
	DrawRect({win->pos + (vector2i_t){win->info.width + 1, 0}, {1,win->info.height + WINDOW_TITLEBAR_HEIGHT + 1}}, WINDOW_BORDER_COLOUR, &renderBuffer); // Right border

	DrawRect({win->pos + (vector2i_t){1,1}, {win->info.width, WINDOW_TITLEBAR_HEIGHT}}, {64, 64, 64}, &renderBuffer);

	DrawString(win->info.title, win->pos.x + 6, win->pos.y + 6, 255, 255, 255, &renderBuffer);

	vector2i_t renderPos = win->pos + (vector2i_t){1, WINDOW_TITLEBAR_HEIGHT + 1};

	syscall(SYS_RENDER_WINDOW, (uintptr_t)&renderBuffer, (uintptr_t)win->handle,(uintptr_t)&renderPos,0,0);
}

int main(){
	fb = lemon_map_fb(&fbInfo); // Request framebuffer mapping from kernel
	surface_t* fbSurface = CreateFramebufferSurface(fbInfo,fb); // Create surface object for framebuffer
	renderBuffer = *fbSurface;
	renderBuffer.buffer = (uint8_t*)malloc(renderBuffer.pitch * renderBuffer.height); // Render buffer/Double buffer

	DrawRect(0,0,fbSurface->width,fbSurface->height,255,0,128, fbSurface);

	syscall(SYS_CREATE_DESKTOP,0,0,0,0,0); // Get Kernel to create Desktop

	int windowCount; // Window Count

	List<Window_s*> windows;
	Window_s* active; // Active Window

	win_info_t testWindow;
	testWindow.width = 100;
	testWindow.height = 100;
	testWindow.x = 10;
	testWindow.y = 10;
	strcpy(testWindow.title, "Test Window");
	testWindow.flags = 0;

	syscall(SYS_CREATE_WINDOW, (uintptr_t)&testWindow,0,0,0,0);

	syscall(SYS_EXEC, (uintptr_t)"/snake.lef",0,0,0,0);

	int mouseDevice = lemon_open("/dev/mouse0", 0);
	lemon_read(mouseDevice, mouseData, 3);

	for(;;){
		DrawRect(0, 0, renderBuffer.width, renderBuffer.height, 64, 128, 128, &renderBuffer);

		int _windowCount; // Updated Window Count
		syscall(SYS_DESKTOP_GET_WINDOW_COUNT, (uintptr_t)(&_windowCount),0,0,0,0);
		if(_windowCount != windowCount){ // Has the window count changed?
			windowCount = _windowCount;
			
			for(int j = 0; j < windowCount; j++){ // Loop through windows and match with what the kernel has
				bool windowFound = false;
				win_info_t windowInfo;
				handle_t windowHandle;

				syscall(SYS_DESKTOP_GET_WINDOW,(uintptr_t)&windowInfo,j,0,0,0);
				windowHandle = windowInfo.handle;

				for(int i = 0; i < windows.get_length(); i++){
					if(windows[i]->handle == windowHandle){ // We already know about this window
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
		}
		
		for(int i = 0; i < windowCount; i++){ // Draw all our windows
			Window_s* win = windows[i];
			DrawWindow(win);
		}

		lemon_read(mouseDevice, mouseData, 3);
		
		mousePos.x += mouseData[1];
		mousePos.y -= mouseData[2];

		if((mouseData[0] & 0x1) && !mouseDown){
			mouseDown = true;
			for(int i = windows.get_length()-1; i >= 0; i--){
				Window_s* win = windows[i];
				if(mousePos.x > win->pos.x && mousePos.x < win->pos.x + win->info.width && mousePos.y > win->pos.y && mousePos.y < win->pos.y + win->info.height){
					windows.add_back(windows.remove_at(i));
					active = windows[i];
					//redrawWindowDecorations = true;
					if(mousePos.y < win->pos.y + 25 && !(win->info.flags & WINDOW_FLAGS_NODECORATION)){
						dragOffset = {mousePos.x - win->pos.x, mousePos.y - win->pos.y};
						drag = true;
					}
					break;
				}
			}
		}

		if(drag){
			active->pos.x = mousePos.x - dragOffset.x;
			active->pos.y = mousePos.y - dragOffset.y;
			if(active->pos.x < 0) active->pos.x = 0;
			if(active->pos.y < 0) active->pos.y = 0;
		}

		if (!(mouseDown) && drag){
			drag = false;
		}

		if(mouseDown && !(mouseData[0] & 0x1)){
			mouseDown = false;
			drag = false;
		}

		if(mouseDown){
			DrawRect(0,300,50,50,255,0,0,&renderBuffer);
		}

		if(mouseData[0] & 0x1){
			DrawRect(50,300,50,50,0,255,0,&renderBuffer);
		}

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

			if(!windowFound) windows.remove_at(i);
		}

		DrawRect(mousePos.x, mousePos.y, 5, 5, 255, 0, 0, &renderBuffer);

		surfacecpy(fbSurface,&renderBuffer); // Render our buffer
	}

	for(;;);
}
