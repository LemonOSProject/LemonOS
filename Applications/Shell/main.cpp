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
#include <fcntl.h>
#include <unistd.h>
#include <lemon/spawn.h>

#define MENU_ITEM_HEIGHT 24

fb_info_t videoInfo;
Lemon::GUI::Window* taskbar;
Lemon::GUI::Window* menu;

bool showMenu = true;

char versionString[80];

struct MenuItem{
	char name[64];
	char path[64];
};

MenuItem menuItems[32];
int menuItemCount = 0;

void OnTaskbarPaint(surface_t* surface){
	Lemon::Graphics::DrawGradientVertical(100,0,surface->width - 100, /*surface->height*/24, {96, 96, 96}, {42, 50, 64},surface);
	Lemon::Graphics::DrawRect(100,24,surface->width - 100, surface->height - 24, {42,50,64},surface);

	if(showMenu){
		Lemon::Graphics::DrawGradientVertical(0,0,100, 24, {120,12,12}, {/*160*/60, /*16*/6, /*16*/6},surface);
		Lemon::Graphics::DrawRect(0,24,100, surface->height - 24, {/*160*/60, /*16*/6, /*16*/6},surface);
	} else {
		Lemon::Graphics::DrawGradientVertical(0,0,100, 24, {220,/*24*/48,30}, {/*160*/120, /*16*/12, /*16*/12},surface);
		Lemon::Graphics::DrawRect(0,24,100, surface->height - 24, {/*160*/120, /*16*/12, /*16*/12},surface);
	}
}

void OnMenuPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0,32,surface->width, surface->height - 32, {64,64,64}, surface);
	Lemon::Graphics::DrawGradientVertical(0,0,surface->width, 32, {220,/*24*/48,30}, {/*160*/120, /*16*/12, /*16*/12},surface);
	Lemon::Graphics::DrawString(versionString,5,MENU_ITEM_HEIGHT / 2 - 6,255,255,255,surface);

	for(int i = 0; i < menuItemCount; i++){
		Lemon::Graphics::DrawString(menuItems[i].name, 5, 42 + i * MENU_ITEM_HEIGHT /* 2 pixels padding */, 255, 255, 255, surface);
	}
}

void LoadConfig(){
	FILE* menuConfig = fopen("/initrd/menu.cfg","r");

	if(!menuConfig){
		return;
	}

	fseek(menuConfig, 0, SEEK_END);
	size_t configSize = ftell(menuConfig);

	fseek(menuConfig, 0, SEEK_SET);

	char* config = (char*)malloc(configSize);

	fread(config, configSize, 1, menuConfig);

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

	taskbar = Lemon::GUI::CreateWindow(&taskbarInfo);
	taskbar->OnPaint = OnTaskbarPaint;

	menu = Lemon::GUI::CreateWindow(&menuInfo);
	menu->OnPaint = OnMenuPaint;

	LoadConfig();

	for(;;){
		Lemon::GUI::PaintWindow(taskbar);

		if(showMenu){
			Lemon::GUI::PaintWindow(menu);
		}

		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			switch (msg.msg)
			{
			case WINDOW_EVENT_MOUSEUP:
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				if(mouseX < 100 && (handle_t)msg.data2 == taskbar->handle){
					showMenu = !showMenu;

					if(!showMenu){
						menu->info.flags |= WINDOW_FLAGS_MINIMIZED;
					} else {
						menu->info.flags &= ~((typeof(menu->info.flags))WINDOW_FLAGS_MINIMIZED);
					}

					Lemon::GUI::UpdateWindow(menu);
				} else if ((handle_t)msg.data2 == menu->handle){
					if(mouseY > 42 && mouseY < (menuItemCount*MENU_ITEM_HEIGHT + 42)){
						char* argv[] = {menuItems[(int)floor((double)(mouseY - 42) / MENU_ITEM_HEIGHT)].path};
						lemon_spawn(argv[0], 1, argv, 0);
						showMenu = false;
						menu->info.flags |= WINDOW_FLAGS_MINIMIZED;
						Lemon::GUI::UpdateWindow(menu);
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
