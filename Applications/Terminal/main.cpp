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
#include <termios.h>
#include <sys/ioctl.h>

#include "escape.h"
#include "colours.h"

#define TASKBAR_COLOUR_R 128
#define TASKBAR_COLOUR_G 128
#define TASKBAR_COLOUR_B 128

#define MENU_WIDTH 200
#define MENU_HEIGHT 300

struct TermState{
	bool bold ;//: 1;
	bool italic ;//: 1;
	bool faint ;//: 1;
	bool underline ;//: 1;
	bool blink ;//: 1;
	bool reverse ;//: 1;
	bool strikethrough ;//: 1;

	char fgColour;
	char bgColour;
};

TermState defaultState {
	.fgColour = 7,
	.bgColour = 0,
};

struct TerminalChar {
	TermState s;
	char c;
};

TermState state = defaultState;

bool escapeSequence = false;
int escapeType = 0;

surface_t menuSurface;
surface_t windowSurface;

#define MAX_LINE_COUNT 10000
#define COLUMN_COUNT 80
TerminalChar* buffer[MAX_LINE_COUNT];
uint8_t bufferLineSize[MAX_LINE_COUNT];

vector2i_t curPos = {0, 0};

const int escBufMax = 256;

char escBuf[escBufMax];

win_info_t windowInfo;
char charactersPerLine;

void OnPaint(surface_t* surface){

	int line = 0;
	int i = 0;
	
	for(int i = 0; i < 25 && i <= curPos.y; i++){
		int _currentLine = (curPos.y < 25) ? 0 : (curPos.y - 24);
		for(int j = 0; j < bufferLineSize[_currentLine + i]; j++){
			TerminalChar ch = buffer[_currentLine + i][j];
			rgba_colour_t fg = colours[ch.s.fgColour];
			rgba_colour_t bg = colours[ch.s.bgColour];
			Lemon::Graphics::DrawRect(j * 8, i * 12, 8, 12, bg.r, bg.g, bg.b, surface);
			Lemon::Graphics::DrawChar(ch.c, j * 8, i * 12, fg.r, fg.g, fg.b, surface);
		}
	}
}

void DoAnsiSGR(){
	int r = -1;
	if(strchr(escBuf, ';')){
		char temp[strchr(escBuf, ';') - escBuf + 1];
		temp[strchr(escBuf, ';') - escBuf] = 0;
		strncpy(temp, escBuf, (int)(strchr(escBuf, ';') - escBuf));
		r = atoi(escBuf);

	} else r = atoi(escBuf);
	if(r < 30){
		switch (r)
		{
		case 0:
			state = defaultState;
			break;
		case 1:
			state.bold = true;
			break;
		case 2:
			state.faint = true;
			break;
		case 3:
			state.italic = true;
			break;
		case 4:
			state.underline = true;
			break;
		case 5:
		case 6:
			state.blink = true;
			break;
		case 7:
			state.reverse = true;
			break;
		case 9:
			state.strikethrough = true;
			break;
		case 21:
			state.underline = true;
			break;
		case 24:
			state.underline = false;
			break;
		case 25:
			state.blink = false;
			break;
		case 27:
			state.reverse = false;
			break;
		default:
			break;
		}
	} else if (r >= ANSI_CSI_SGR_FG_BLACK && r <= ANSI_CSI_SGR_FG_WHITE){ // Foreground Colour
		state.fgColour = r - ANSI_CSI_SGR_FG_BLACK;
	} else if (r >= ANSI_CSI_SGR_BG_BLACK && r <= ANSI_CSI_SGR_BG_WHITE){ // Background Colour
		state.bgColour = r - ANSI_CSI_SGR_BG_BLACK;
	} else if (r >= ANSI_CSI_SGR_FG_BLACK_BRIGHT && r <= ANSI_CSI_SGR_FG_WHITE_BRIGHT){ // Foreground Colour (Bright)
		state.fgColour = r - ANSI_CSI_SGR_FG_BLACK_BRIGHT + 8;
	} else if (r >= ANSI_CSI_SGR_BG_BLACK_BRIGHT && r <= ANSI_CSI_SGR_BG_WHITE_BRIGHT){ // Background Colour (Bright)
		state.bgColour = r - ANSI_CSI_SGR_BG_BLACK_BRIGHT + 8;
	} else if (r == ANSI_CSI_SGR_FG){
		if(strchr(escBuf, ';')){
			state.fgColour = atoi(strchr(escBuf, ';') + 1); // Get argument
		}
	} else if (r == ANSI_CSI_SGR_BG){
		if(strchr(escBuf, ';')){
			state.bgColour = atoi(strchr(escBuf, ';') + 1); // Get argument
		}
	}
}

void DoAnsiCSI(char ch){
	switch(ch){
	case ANSI_CSI_SGR:
		DoAnsiSGR(); // Set Graphics Rendition
		break;
	case ANSI_CSI_CUU:
		curPos.y--;
		if(curPos.y < 0) curPos.y = 0;
		break;
	case ANSI_CSI_CUD:
		curPos.y++;
		break;
	case ANSI_CSI_CUF:
		curPos.x++;
		if(curPos.x >= COLUMN_COUNT) {
			curPos.x = 0;
			curPos.y++;
		}
		break;
	case ANSI_CSI_CUB:
		curPos.x--;
		if(curPos.x < 0) {
			curPos.x = COLUMN_COUNT - 1;
			curPos.y--;
		}
		if(curPos.y < 0) curPos.y = 0;
		break;
	}
}

void DoAnsiOSC(char ch){

}

void PrintChar(char ch){
	if(escapeSequence){
		if(!escapeType){
			escapeType = ch;
			ch = 0;
		}
		
		if(escapeType == ANSI_CSI || escapeType == ANSI_OSC){
			if(!isalpha(ch)){ // Found output sequence
				if(strlen(escBuf) >= escBufMax){
					escapeSequence = false;
					return;
				}
				char s[] = {ch, 0};
				strncat(escBuf, s, 2); // Add to buff
				return;
			}

			if(escapeType == ANSI_CSI){
				DoAnsiCSI(ch);
			} else if (escapeType == ANSI_OSC) {
				DoAnsiOSC(ch);
			}
		} else if (escapeType == ANSI_RIS){
			state = defaultState;
		}
		escapeSequence = 0;
		escapeType = 0;
	} else {
		if(!bufferLineSize[curPos.y]){
			bufferLineSize[curPos.y] = COLUMN_COUNT;
			buffer[curPos.y] = (TerminalChar*)malloc(COLUMN_COUNT * sizeof(TerminalChar));
			memset(buffer[curPos.y], 0, bufferLineSize[curPos.y] * sizeof(TerminalChar));
		}

		switch (ch)
		{
		case ANSI_ESC:
			escapeSequence = true;
			escapeType = 0;
			escBuf[0] = 0;
			break;
		case '\n':
			curPos.y++;
			curPos.x = 0;
			break;
		case '\b':
			if(curPos.x > 0) curPos.x--;
			else if(curPos.y > 0) {
				curPos.y--;
				curPos.x = COLUMN_COUNT - 1;
			}
			
			buffer[curPos.y][curPos.x].s = state;
			buffer[curPos.y][curPos.x].c = 0;
			curPos.x++;
			break;
		case ' ':
			if(curPos.x >= COLUMN_COUNT){
				curPos.y++;
				curPos.x = 0;
			}
			buffer[curPos.y][curPos.x].s = state;
			buffer[curPos.y][curPos.x].c = ' ';
			curPos.x++;
			break;
		default:
			if(!(isgraph(ch))) break;

			if(curPos.x >= COLUMN_COUNT){
				curPos.y++;
				curPos.x = 0;
			}
			buffer[curPos.y][curPos.x].s = state;
			buffer[curPos.y][curPos.x].c = ch;
			curPos.x++;
			break;
		}
	}
}

extern "C"
int main(char argc, char** argv){
	Lemon::GUI::Window* window;

	windowInfo.width = 640;
	windowInfo.height = 312;
	windowInfo.x = 0;
	windowInfo.y = 0;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "Terminal");
	window = Lemon::GUI::CreateWindow(&windowInfo);
	window->background = {0,0,0,255};

	memset(buffer, 0, sizeof(TerminalChar*)*MAX_LINE_COUNT);
	memset(bufferLineSize, 0, sizeof(int) * MAX_LINE_COUNT);
	curPos = {0, 0};

	int masterPTYFd;
	syscall(SYS_GRANT_PTY, (uintptr_t)&masterPTYFd, 0, 0, 0, 0);

	syscall(SYS_EXEC, (uintptr_t)"/initrd/lsh.lef", 0, 0, 1 /* Duplicate File Descriptors */, 0);
	
	window->OnPaint = OnPaint;

	char* _buf = (char*)malloc(512);

	Lemon::Graphics::LoadFont("/initrd/sourcecodepro.ttf");

	winsize wSz = {
		.ws_row = 25,
		.ws_col = COLUMN_COUNT,
		.ws_xpixel = windowInfo.width,
		.ws_ypixel = windowInfo.height,
	};

	ioctl(masterPTYFd, TIOCSWINSZ, &wSz);

	for(;;){
		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_KEY){
				if(msg.data < 128){
					char key = (char)msg.data;
					char b[] = {key, key, 0};
					lemon_write(masterPTYFd, b, 1);
					//PrintChar(key);
				}
			} else if (msg.msg == WINDOW_EVENT_CLOSE){
				Lemon::GUI::DestroyWindow(window);
				free(_buf);
				exit(0);
			}
		}

		while(int len = lemon_read(masterPTYFd, _buf, 512)){
			for(int i = 0; i < len; i++){
				PrintChar(_buf[i]);
			}
		}
		
		Lemon::GUI::PaintWindow(window);
	}
	for(;;);
}
