#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <core/ipc.h>
#include <core/keyboard.h>
#include <stdlib.h>
#include <stdio.h>
#include <list.h>

#define TASKBAR_COLOUR_R 128
#define TASKBAR_COLOUR_G 128
#define TASKBAR_COLOUR_B 128

#define MENU_WIDTH 200
#define MENU_HEIGHT 300

typedef struct fs_dirent {
	uint32_t inode; // Inode number
	char name[128]; // Filename
} fs_dirent_t;

surface_t menuSurface;
surface_t windowSurface;

extern "C"
void pmain(){

	Window* taskbarWindow;
	Window* menuWindow;
	win_info_t menuWindowInfo;
	win_info_t windowInfo;

	video_mode_t videoMode = GetVideoMode();

	windowInfo.width = videoMode.width;
	windowInfo.height = 32;
	windowInfo.x = 0;
	windowInfo.y = videoMode.height - 32;
	windowInfo.flags = WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_SNAP_TO_BOTTOM;
	taskbarWindow = CreateWindow(&windowInfo);

	menuWindowInfo.width = MENU_WIDTH;
	menuWindowInfo.height = MENU_HEIGHT;
	menuWindowInfo.x = 0;
	menuWindowInfo.y = videoMode.height - 32 - MENU_HEIGHT;
	menuWindowInfo.flags = WINDOW_FLAGS_NODECORATION;

	bool showMenu = false;
	bool btnDown = false;

	int mX;
	int mY;

	FILE* menuIndexFile = fopen("/menu.txt","r");

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch(msg.data){
				default:
					break;
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEUP){
				btnDown = false;
				if(mX > 4 && mY > 4 && mX < (4 + 24) && mY < (4 + 24)){
					showMenu = !showMenu;
					if(showMenu){
						menuWindow = CreateWindow(&menuWindowInfo);
					} else if(menuWindow) {
						DestroyWindow(menuWindow);
						menuWindow = NULL;
					}
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;
				
				mX = mouseX;
				mY = mouseY;

				if(mX > 4 && mY > 4 && mX < (4 + 24) && mY < (4 + 24)){
					btnDown = true;
				}
			}
		}
	
		if(showMenu){
			PaintWindow(menuWindow);
			//DrawRect(0, 0, MENU_WIDTH, MENU_HEIGHT, TASKBAR_COLOUR_R, TASKBAR_COLOUR_G, TASKBAR_COLOUR_B, &menuSurface);
			//PaintWindow(menuWindow, &menuSurface);
			//syscall(SYS_PAINT_WINDOW, (uint32_t)menuWindow, (uint32_t)&menuSurface,0,0,0);
		}

		DrawRect(0,0,windowSurface.width, 32, TASKBAR_COLOUR_R, TASKBAR_COLOUR_G, TASKBAR_COLOUR_B,&taskbarWindow->surface);
		DrawRect(0,0,windowSurface.width, 1, 0, 0, 0, &taskbarWindow->surface);
		DrawRect(0, 0, 1, 32, 0, 0, 0, &taskbarWindow->surface);
		DrawRect(windowSurface.width-1,0,1, 32, 0, 0, 0, &taskbarWindow->surface);
		DrawRect(0,32-1,windowSurface.width,1, 0, 0, 0, &taskbarWindow->surface);

		DrawRect(3,3,26,26,0,0,0,&taskbarWindow->surface);
		DrawRect(4,4,24,24,showMenu ? 64 : TASKBAR_COLOUR_R,showMenu ? 64 : TASKBAR_COLOUR_G,showMenu ? 64 : TASKBAR_COLOUR_B,&taskbarWindow->surface);

		//PaintWindow(taskbarWindow);
		syscall(SYS_PAINT_WINDOW, (uint32_t)taskbarWindow->handle, (uint32_t)&taskbarWindow->surface,0,0,0);
	}

	for(;;);
}

/*
extern "C"
void pmain(){

	win_info_t windowInfo;
	handle_t windowHandle;
	win_info_t menuWindowInfo;

	video_mode_t videoMode = GetVideoMode();

	windowInfo.width = videoMode.width;
	windowInfo.height = 32;
	windowInfo.x = 0;
	windowInfo.y = videoMode.height - 32;
	windowInfo.flags = WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_SNAP_TO_BOTTOM;
	windowHandle = CreateWindow(&windowInfo);

	menuWindowInfo.width = MENU_WIDTH;
	menuWindowInfo.height = MENU_HEIGHT;
	menuWindowInfo.x = 0;
	menuWindowInfo.y = videoMode.height - 32 - MENU_HEIGHT;
	menuWindowInfo.flags = WINDOW_FLAGS_NODECORATION;

	windowSurface.buffer = (uint8_t*)malloc(videoMode.width*32*4);
	windowSurface.width = videoMode.width;
	windowSurface.height = 32;

	menuSurface.buffer = (uint8_t*)malloc(MENU_WIDTH*MENU_HEIGHT*4);
	menuSurface.width = MENU_WIDTH;
	menuSurface.height = MENU_HEIGHT;

	handle_t menuWindow;

	bool showMenu = false;
	bool btnDown = false;

	int mX;
	int mY;

	FILE* menuIndexFile = fopen("/menu.txt","r");

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch(msg.data){
				default:
					break;
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEUP){
				btnDown = false;
				if(mX > 4 && mY > 4 && mX < (4 + 24) && mY < (4 + 24)){
					showMenu = !showMenu;
					if(showMenu){
						menuWindow = CreateWindow(&menuWindowInfo);
					} else if(menuWindow) {
						DestroyWindow(menuWindow);
						menuWindow = NULL;
					}
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;
				
				mX = mouseX;
				mY = mouseY;

				if(mX > 4 && mY > 4 && mX < (4 + 24) && mY < (4 + 24)){
					btnDown = true;
				}
			}
		}
	
		if(showMenu){
			DrawRect(0, 0, MENU_WIDTH, MENU_HEIGHT, TASKBAR_COLOUR_R, TASKBAR_COLOUR_G, TASKBAR_COLOUR_B, &menuSurface);
			PaintWindow(menuWindow, &menuSurface);
			//syscall(SYS_PAINT_WINDOW, (uint32_t)menuWindow, (uint32_t)&menuSurface,0,0,0);
		}

		DrawRect(0,0,windowSurface.width, 32, TASKBAR_COLOUR_R, TASKBAR_COLOUR_G, TASKBAR_COLOUR_B,&windowSurface);
		DrawRect(0,0,windowSurface.width, 1, 0, 0, 0, &windowSurface);
		DrawRect(0, 0, 1, 32, 0, 0, 0, &windowSurface);
		DrawRect(windowSurface.width-1,0,1, 32, 0, 0, 0, &windowSurface);
		DrawRect(0,32-1,windowSurface.width,1, 0, 0, 0, &windowSurface);

		DrawRect(3,3,26,26,0,0,0,&windowSurface);
		DrawRect(4,4,24,24,showMenu ? 64 : TASKBAR_COLOUR_R,showMenu ? 64 : TASKBAR_COLOUR_G,showMenu ? 64 : TASKBAR_COLOUR_B,&windowSurface);

		syscall(SYS_PAINT_WINDOW, (uint32_t)windowHandle, (uint32_t)&windowSurface,0,0,0);
	}

	for(;;);
}
*/