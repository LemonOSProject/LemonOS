#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <core/filesystem.h>
#include <core/lemon.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

char* menuFileData;
int menuFileSize;
video_mode_t videoMode;

surface_t menuSurface;
bool showMenu = false;
win_info_t menuWindowInfo;

struct MenuEntry{
	char name[128];
	char path[128];
};

int menuEntryCount;
MenuEntry menuEntries[20];

void OnPaint(surface_t* surface){
	DrawGradientVertical(0,0,videoMode.width,8, {72,72,72,255}, {48,48,48,255}, surface);
	DrawRect(0,8,videoMode.width,24,{48, 48, 48, 255}, surface);

	DrawGradientVertical(0,0,128,8, {showMenu ? 96 : 192,showMenu ? 64 : 128,showMenu ? 36 : 72,255}, {showMenu ? 72 : 144,showMenu ? 36 : 72,showMenu ? 16 : 32,255}, surface);
	DrawRect(0,8,128,24,{showMenu ? 72 : 144, showMenu ? 36 : 72, showMenu ? 16 : 32, 255}, surface);
}

void PaintMenu(surface_t* surface){
	DrawRect(0,0,menuWindowInfo.width,menuWindowInfo.height,{48,48,48,255},surface);
	DrawString("Lemon OS",2,2,255,255,255,surface);
	
	for(int i = 0; i < menuEntryCount;i++){
		DrawString(menuEntries[i].name,2,32 + i*16,255,255,255,surface);
	}
}

void CalcGradient(int width, int height){
	//gradient.width = width;
	//gradient.height = height;
	//gradient.buffer = (uint8_t*)malloc(width*height*4);
	//DrawGradientVertical(0,0,width,height/2,{48,48,48,255},{96,96,96,255},&gradient);
	//DrawGradientVertical(0,height/2-1,width,height/2+1 /* Avoid pixels of same colour next to each other*/,{96,96,96,255},{48,48,48,255},&gradient);
}

void ParseMenu(){
	char name[128];
	char buffer[128];
	int namePos = 0;
	int bufPos = 0;
	memset(buffer,0,128);
	memset(name,0,128);
	for(int i = 0; i < menuFileSize; i++){
		switch(menuFileData[i]){
		case '\r':
			break;
		case '\n':
			if(!namePos){
				namePos = 0;
				bufPos = 0;
				break;
			}
			menuEntryCount++;
			//if(!menuEntries) menuEntries = (MenuEntry*)malloc(sizeof(MenuEntry));
			//else menuEntries = (MenuEntry*)realloc(menuEntries,sizeof(MenuEntry) * menuEntryCount);
			
			strcpy(menuEntries[menuEntryCount-1].name, name);
			strcpy(menuEntries[menuEntryCount-1].path, buffer);
			
			memset(buffer,0,128);
			memset(name,0,128);
			namePos = 0;
			bufPos = 0;
			break;
		case ':':
			strncpy(name, buffer, bufPos);
			namePos = bufPos;
			bufPos = 0;
			memset(buffer,0,128);
			break;
		default:
			buffer[bufPos++] = menuFileData[i];
			break;
		}
	}
}

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;
	videoMode = GetVideoMode();

	windowInfo.width = videoMode.width;
	windowInfo.height = 64;
	windowInfo.x = 0;
	windowInfo.y = videoMode.height - 32;
	windowInfo.flags = WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOBACKGROUND;
	
	menuWindowInfo.width = 192;
	menuWindowInfo.height = 256;
	menuWindowInfo.x = 0;
	menuWindowInfo.y = videoMode.height - 32 - 256;
	menuWindowInfo.flags = WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOBACKGROUND;

	menuSurface.width = menuWindowInfo.width;
	menuSurface.height = menuWindowInfo.height;

	/*int horizontalSizePadded = menuWindowInfo.width * 4 + (0x10 - ((menuWindowInfo.width * 4) % 0x10));
	{
		menuSurface.buffer = (uint8_t*)malloc(horizontalSizePadded * menuWindowInfo.height);
		menuSurface.linePadding = (0x10 - ((menuWindowInfo.width * 4) % 0x10));
	}*/
	menuSurface.buffer = (uint8_t*)malloc(menuWindowInfo.width*menuWindowInfo.height*4);

	Window* win = CreateWindow(&windowInfo);
	win->OnPaint = OnPaint;

	handle_t menuWindow = 0;

	uint16_t mouseX;
	uint16_t mouseY;

	int menuFile = open("/menu.txt",0);
	menuFileSize = lseek(menuFile, 0, SEEK_END);
	menuFileData = (char*)malloc(menuFileSize);
	read(menuFile,menuFileData,menuFileSize);
	close(menuFile);

	ParseMenu();

	PaintWindow(win);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch(msg.data){
				default:
					break;
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEUP){
				
				if(mouseY > 32 && showMenu){// && msg.data2 == (uint32_t)menuWindow){
					int index = (mouseY-32)/16;
					if(index > menuEntryCount){
						continue;
					} else {
						syscall(SYS_EXEC,(uint32_t)menuEntries[index].path,0,0,0,0);
					}
					continue;
				} else if(mouseX < 128 && mouseY < 32){// && msg.data2 == (uint32_t)win->handle){
					showMenu = !showMenu;

					if(showMenu){
						menuWindow = _CreateWindow(&menuWindowInfo);
						PaintMenu(&menuSurface);
						_PaintWindow(menuWindow,&menuSurface);
					}
					else {
						_DestroyWindow(menuWindow);
						menuWindow = NULL;
					}
					PaintWindow(win);
					break;
				}
				//HandleMouseUp(win);
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				mouseX = msg.data >> 16;
				mouseY = msg.data & 0xFFFF;

				//HandleMouseDown(win, {mouseX, mouseY});
				PaintWindow(win);
				if(showMenu) {
					PaintMenu(&menuSurface);
					_PaintWindow(menuWindow,&menuSurface);
				}
			}
		}
	}
}