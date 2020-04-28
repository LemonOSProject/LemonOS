#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/ipc.h>
#include <lemon/keyboard.h>
#include <stdlib.h>
#include <stdio.h>
#include <list.h>
#include <lemon/filesystem.h>
#include <lemon/spawn.h>
#include <ctype.h>

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

char buffer[1000][80];
int currentLine = 0;
int bufferPos = 0;

win_info_t windowInfo;
char charactersPerLine;

void OnPaint(surface_t* surface){

	int line = 0;
	int i = 0;
	
	for(int i = 0; i < 25 && i <= currentLine; i++){
		int _currentLine = (currentLine < 25) ? 0 : (currentLine - 24);
		for(int j = 0; j < 80; j++){
			DrawChar(buffer[_currentLine + i][j], j * 8, i * 12, 255, 255, 255, surface);
		}
	}
}

void PrintChar(char ch){
	switch (ch)
	{
	case '\n':
		currentLine++;
		bufferPos = 0;
		break;
	case '\b':
		if(bufferPos > 0) bufferPos--;
		else if(currentLine > 0) {
			currentLine--;
			bufferPos = 79;
		}
		
		buffer[currentLine][bufferPos] = 0;
		break;
	case ' ':
		if(bufferPos >= 80){
			currentLine++;
			bufferPos = 0;
		}
		buffer[currentLine][bufferPos++] = ' ';
		break;
	default:
		if(!(isgraph(ch))) break;

		if(bufferPos >= 80){
			currentLine++;
			bufferPos = 0;
		}
		buffer[currentLine][bufferPos++] = ch;
		break;
	}
}

extern "C"
int main(char argc, char** argv){
	Window* window;

	windowInfo.width = 640;
	windowInfo.height = 300;
	windowInfo.x = 0;
	windowInfo.y = 0;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "Terminal");
	window = CreateWindow(&windowInfo);
	window->background = {0,0,0,255};

	charactersPerLine = 80;

	int masterPTYFd;
	syscall(SYS_GRANT_PTY, (uintptr_t)&masterPTYFd, 0, 0, 0, 0);

	syscall(SYS_EXEC, (uintptr_t)"/initrd/lsh.lef", 0, 0, 1 /* Duplicate File Descriptors */, 0);
	
	window->OnPaint = OnPaint;

	char* _buf = (char*)malloc(512);

	LoadFont("/initrd/sourcecodepro.ttf");

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_KEY){
				if(msg.data < 128){
					char key = (char)msg.data;
					char b[] = {key, key, 0};
					lemon_write(masterPTYFd, b, 1);
					//PrintChar(key);
				}
			} else if (msg.msg == WINDOW_EVENT_CLOSE){
				DestroyWindow(window);
				free(_buf);
				exit(0);
			}
		}

		while(int len = lemon_read(masterPTYFd, _buf, 512)){
			for(int i = 0; i < len; i++){
				PrintChar(_buf[i]);
			}
		}
		
		PaintWindow(window);
	}
	for(;;);
}
