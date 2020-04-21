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
		int _currentLine = (currentLine < 25) ? 0 : currentLine - 25;
		for(int j = 0; j < 80; j++){
			DrawChar(buffer[_currentLine + i][j], j * 8, i * 12, 255, 255, 255, surface);
		}
	}

	//char temp[10];
	//DrawString(itoa(bufferPos, temp, 10), 0, 0, 255, 0, 0, surface);
	//DrawString(itoa(currentLine, temp, 10), 0, 16, 255, 0, 0, surface);
}

extern "C"
int main(char argc, char** argv){
	Window* window;

	windowInfo.width = 640;
	windowInfo.height = 300;
	windowInfo.x = 0;
	windowInfo.y = 0;
	windowInfo.flags = 0;
	window = CreateWindow(&windowInfo);
	window->background = {0,0,0,255};

	charactersPerLine = 80;

	int masterPTYFd;
	syscall(SYS_GRANT_PTY, (uintptr_t)&masterPTYFd, 0, 0, 0, 0);

	syscall(SYS_EXEC, (uintptr_t)"/lsh.lef", 0, 0, 1, 0);
	
	window->OnPaint = OnPaint;

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_KEY){
				if(msg.data < 256){
					char key = (char)msg.data;
					//lemon_write(masterPTYFd, &key, 1);
				}
			} else if (msg.msg == WINDOW_EVENT_CLOSE){
				DestroyWindow(window);
				return 0;
			}
		}
		int num = lemon_read(masterPTYFd, &buffer[currentLine][bufferPos], 1);
		do{
		if(num){
			bufferPos++;
			if(buffer[currentLine][bufferPos-1] == '\n' || bufferPos >= 80){
				bufferPos = 0;
				currentLine++;
			}
		}
		} while (num = lemon_read(masterPTYFd, &buffer[currentLine][bufferPos], 1));
		
		PaintWindow(window);
	}
	for(;;);
}
