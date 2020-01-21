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

fb_info_t videoInfo;
Window* taskbar;
Window* menu;

bool showMenu = true;

char versionString[80];

struct MenuItem{
	char name[64];
	char path[64];
};

MenuItem menuItems[32];
int menuItemCount = 0;

void OnTaskbarPaint(surface_t* surface){
	DrawGradientVertical(100,0,surface->width - 100, surface->height, {96,96,96}, {64, 64, 64},surface);
	DrawGradientVertical(0,0,100, surface->height, {240,24,24}, {/*160*/120, /*16*/12, /*16*/12},surface);
}

void OnMenuPaint(surface_t* surface){
	DrawRect(0,32,surface->width, surface->height - 32, {64,64,64}, surface);
	DrawGradientVertical(0,0,surface->width, 32, {240,24,24}, {/*160*/120, /*16*/12, /*16*/12},surface);
	DrawString(versionString,5,10,255,255,255,surface);

	for(int i = 0; i < menuItemCount; i++){
		DrawString(menuItems[i].name, 5, 42 + i * 14 /* 2 pixels padding */, 255, 255, 255, surface);
	}
}

void LoadConfig(){
	int menuConfig = lemon_open("/menu.cfg",0);

	size_t configSize = lemon_seek(menuConfig, 0, SEEK_END);

	lemon_seek(menuConfig, 0, SEEK_SET);

	char* config = (char*)malloc(configSize);

	lemon_read(menuConfig, config, configSize);

	char name[64];
	char buffer[64];
	int namePos = 0;
	int bufPos = 0;
	memset(buffer,0,64);
	memset(name,0,64);
	for(int i = 0; i < configSize; i++){
		switch(config[i]){
		case '\r':
			break;
		case '\n':
			if(!namePos){
				namePos = 0;
				bufPos = 0;
				break;
			}
			menuItemCount++;
			
			strcpy(menuItems[menuItemCount-1].name, name);
			strcpy(menuItems[menuItemCount-1].path, buffer);
			
			memset(buffer,0,64);
			memset(name,0,64);
			namePos = 0;
			bufPos = 0;
			break;
		case ':':
			strncpy(name, buffer, bufPos);
			namePos = bufPos;
			bufPos = 0;
			memset(buffer,0,64);
			break;
		default:
			buffer[bufPos++] = config[i];
			break;
		}
	}
}

int main(){
	syscall(SYS_GET_VIDEO_MODE, (uintptr_t)&videoInfo,0,0,0,0);
	syscall(SYS_UNAME, (uintptr_t)versionString,0,0,0,0);

	win_info_t taskbarInfo;
	taskbarInfo.width = videoInfo.width;
	taskbarInfo.height = 30;
	taskbarInfo.x = 0;
	taskbarInfo.y = videoInfo.height - taskbarInfo.height;
	taskbarInfo.flags = WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_SNAP_TO_BOTTOM;

	win_info_t menuInfo;
	menuInfo.width = 240;
	menuInfo.height = 300;
	menuInfo.x = 0;
	menuInfo.y = videoInfo.height - menuInfo.height - taskbarInfo.height;
	menuInfo.flags = WINDOW_FLAGS_NODECORATION;

	taskbar = CreateWindow(&taskbarInfo);
	taskbar->OnPaint = OnTaskbarPaint;

	menu = CreateWindow(&menuInfo);
	menu->OnPaint = OnMenuPaint;

	LoadConfig();

	for(;;){
		PaintWindow(taskbar);

		if(showMenu){
			PaintWindow(menu);
		}

		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			switch (msg.msg)
			{
			case WINDOW_EVENT_MOUSEDOWN:
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data;
				if(mouseX < 128 && (handle_t)msg.data2 == taskbar->handle){
					showMenu = !showMenu;

					if(!showMenu){
						_DestroyWindow(menu->handle);
					} else {
						menu->handle = _CreateWindow(&menu->info);
					}
				} else if ((handle_t)msg.data2 == menu->handle){
					if(mouseY > 42 && mouseY < (menuItemCount*14 - 42)){
						syscall(SYS_EXEC,(uintptr_t)menuItems[(mouseY - 42) / 14].path,0,0,0,0);
					}
				}
				break;
			
			default:
				break;
			}
		}
	}

	for(;;);
}
