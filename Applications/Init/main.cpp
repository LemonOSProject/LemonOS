#include <stdint.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <gfx/graphics.h>
#include <lemon/filesystem.h>
#include <gui/window.h>
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
#include <lemon/sharedmem.h>
#include <lemon/spawn.h>
#include FT_FREETYPE_H

#include "clip.h"
#include "main.h"

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

int frameLen = 1000000000 / 120;

timespec timer;

rgba_colour_t backgroundColor = {64, 128, 128};
surface_t closeButtonSurface;
surface_t backgroundImageSurface;
	
std::vector<rect_t> clipRects;

window_list_t* windowList;

int windowCount; // Window Count
List<Window*> windows;
Window* active; // Active Window

void AddClip(rect_t rect){
retry:
	for(int i = 0; i < clipRects.size(); i++){
		rect_t clip = clipRects[i];
		if(!(GET_LEFT(clip) <= GET_RIGHT(rect) && GET_RIGHT(clip) >= GET_LEFT(rect) && GET_TOP(clip) <= GET_BOTTOM(rect) && GET_BOTTOM(clip) >= GET_TOP(rect)))
		{	
			continue;
		}

		clipRects.erase(clipRects.begin() + i);
		SplitRect(clip, rect, &clipRects);
		goto retry;
		break;
	}

	clipRects.push_back(rect);
}

void RemoveDestroyedWindows(){
	windowCount = windowList->windowCount;
	for(int i = 0; i < windows.get_length(); i++){ // Remove any windows that no longer exist
			bool windowFound = false;
			for(int j = 0; j < windowCount; j++){
				if(windows[i]->handle == windowList->windows[j]){
					windowFound = true;
					break;
				}
			}

			if(!windowFound) {
				Window* w = windows.remove_at(i);
				Lemon::UnmapSharedMemory(w->primaryBuffer, w->info.primaryBufferKey);
				Lemon::UnmapSharedMemory(w->secondaryBuffer, w->info.secondaryBufferKey);
				delete w;
				redrawWindowDecorations = true;
			}
		}
}

void AddNewWindows(){
	windowCount = windowList->windowCount;
	
	for(int j = 0; j < windowCount; j++){ // Loop through windows and match with what the kernel has
		bool windowFound = false;
		handle_t windowHandle = windowList->windows[j];
		win_info_t windowInfo;
		syscall(SYS_DESKTOP_GET_WINDOW, &windowInfo, windowHandle, 0, 0, 0);

		for(int i = 0; i < windows.get_length(); i++){
			if(windows[i]->handle == windowHandle){ // We already know about this window
				windows[i]->info = windowInfo;
				windowFound = true;
				break;
			}
		}

		if(!windowFound){ // New window?
			Window* win = (Window*)malloc(sizeof(Window));
			win->info = windowInfo;
			win->handle = windowHandle;
			win->pos = {win->info.x, win->info.y};
			win->primaryBuffer = (uint8_t*)Lemon::MapSharedMemory(win->info.primaryBufferKey);
			win->secondaryBuffer = (uint8_t*)Lemon::MapSharedMemory(win->info.secondaryBufferKey);
			windows.add_back(win);
	
			redrawWindowDecorations = true;
		}
	}
}

bool PointInWindow(Window* win, vector2i_t point){
	int windowHeight = (win->info.flags & WINDOW_FLAGS_NODECORATION) ? win->info.height : (win->info.height + 25); // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos},{win->info.width, windowHeight}}, point);
}

bool PointInWindowProper(Window* win, vector2i_t point){
	int windowYOffset = (win->info.flags & WINDOW_FLAGS_NODECORATION) ? 0 : 25; // Account for titlebar
	return Lemon::Graphics::PointInRect({{win->pos + (vector2i_t){0, windowYOffset}},{win->info.width, win->info.height}}, point);
}

void memcpy_optimized(void* dest, void* src, size_t count);
int main(){
	fb = lemon_map_fb(&fbInfo); // Request framebuffer mapping from kernel
	surface_t* fbSurface = Lemon::Graphics::CreateFramebufferSurface(fbInfo,fb); // Create surface object for framebuffer
	renderBuffer = *fbSurface;
	renderBuffer.buffer = (uint8_t*)malloc(renderBuffer.pitch * renderBuffer.height); // Render buffer/Double buffer

	Lemon::Graphics::DrawRect(0,0,fbSurface->width,fbSurface->height,255,0,128, fbSurface);

	syscall(SYS_CREATE_DESKTOP,&windowList,0,0,0,0); // Get Kernel to create Desktop

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
	#endif

	lemon_spawn("/initrd/shell.lef", 0, 0, 0);
	lemon_spawn("/initrd/fileman.lef", 0, 0, 0);

	int mouseDevice = lemon_open("/dev/mouse0", 0);
	lemon_read(mouseDevice, mouseData, 3);

	Lemon::Graphics::DrawRect(0, 0, renderBuffer.width, renderBuffer.height, backgroundColor, &renderBuffer);

	for(;;){

		if(redrawWindowDecorations){
			#ifdef ENABLE_BACKGROUND_IMAGE
			Lemon::Graphics::surfacecpy(&renderBuffer, &backgroundImageSurface);
			#else
			Lemon::Graphics::DrawRect(0, 0, renderBuffer.width, renderBuffer.height, backgroundColor, &renderBuffer);
			#endif
		}
		redrawWindowDecorations = false;

		lemon_read(mouseDevice, mouseData, 3);
		
		mousePos.x += mouseData[1] * 3;
		mousePos.y -= mouseData[2] * 3;

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
				Window* win = windows[i];
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
					HandleKeyEvent(msg, active);
					break;
				case DESKTOP_EVENT:
					break;
			}
		}

		if(windowList->dirty){
			windowList->dirty = 2;
			RemoveDestroyedWindows();
			AddNewWindows();
			windowList->dirty = 0;
		}
		
		//timespec newTime;
		//clock_gettime(CLOCK_MONOTONIC, &newTime);
		{//if(((newTime.tv_sec - timer.tv_sec) * 1000000000 + (newTime.tv_nsec - timer.tv_nsec)) >= frameLen){
			//timer = newTime;
			for(int i = 0; i < windowCount; i++){ // Draw all our windows
				Window* win = windows[i];
				win->DrawWindow(&renderBuffer);
			}

			#ifdef ENABLE_FRAMERATE_COUNTER
			frameCounter++;
			UpdateFrameRate();

			Lemon::Graphics::DrawRect(0,0,8*3,12,0,0,0,&renderBuffer);
			char temp[5];
			memset(temp,5,0);
			itoa(frameRate, temp, 10);
			Lemon::Graphics::DrawString(temp,0,0,255,255,255,&renderBuffer);
			#endif
			
			Lemon::Graphics::surfacecpyTransparent(&renderBuffer, &mouseSurface, {mousePos.x, mousePos.y});

			memcpy_optimized(fbSurface->buffer, renderBuffer.buffer, fbInfo.width * fbInfo.height << 2); // Render our buffer
			
			Lemon::Graphics::surfacecpy(fbSurface, &renderBuffer, mousePos, {mousePos, {mouseSurface.width, mouseSurface.height}});

			#ifdef ENABLE_BACKGROUND_IMAGE
			Lemon::Graphics::surfacecpy(&renderBuffer, &backgroundImageSurface, mousePos, {mousePos, {mouseSurface.width, mouseSurface.height}});
			#else
			Lemon::Graphics::DrawRect(mousePos.x, mousePos.y, mouseSurface.width, mouseSurface.height, backgroundColor, &renderBuffer);
			#endif
		}
	}

	for(;;);
}
