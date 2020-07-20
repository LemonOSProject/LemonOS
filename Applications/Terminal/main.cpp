#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/window.h>
#include <core/keyboard.h>
#include <stdlib.h>
#include <stdio.h>
#include <list.h>
#include <lemon/filesystem.h>
#include <lemon/spawn.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <lemon/util.h>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

#include "escape.h"
#include "colours.h"

#define TASKBAR_COLOUR_R 128
#define TASKBAR_COLOUR_G 128
#define TASKBAR_COLOUR_B 128

#define MENU_WIDTH 200
#define MENU_HEIGHT 300

Lemon::GUI::Window* window;

struct TermState{
	bool bold : 1;
	bool italic : 1;
	bool faint : 1;
	bool underline : 1;
	bool blink : 1;
	bool reverse : 1;
	bool strikethrough: 1;

	uint8_t fgColour;
	uint8_t bgColour;
};

TermState defaultState {
	.fgColour = 7,
	.bgColour = 0,
};

struct TerminalChar {
	TermState s;
	char c;
};

Lemon::Graphics::Font* terminalFont;

TermState state = defaultState;

bool escapeSequence = false;
int escapeType = 0;

surface_t menuSurface;
surface_t windowSurface;

int bufferOffset = 0;
int columnCount = 80;
int rowCount = 25;
std::vector<std::vector<TerminalChar>> buffer;

vector2i_t curPos = {0, 0};
vector2i_t storedCurPos = {0, 0};

const int escBufMax = 256;

char escBuf[escBufMax];

char charactersPerLine;

void Scroll(){
	if(curPos.y >= rowCount){
		bufferOffset += curPos.y - (rowCount - 1);
		curPos.y = rowCount - 1;
	}
			
	while(bufferOffset + rowCount >= buffer.size()) buffer.push_back(std::vector<TerminalChar>());
}

void OnPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, 0, window->GetSize().x, window->GetSize().y, 0, 0, 0, surface);

	int line = 0;
	int i = 0;
	int fontHeight = terminalFont->height;

	for(int i = 0; i < rowCount && (bufferOffset + i) < buffer.size(); i++){
		for(int j = 0; j < buffer[bufferOffset + i].size(); j++){
			TerminalChar ch = buffer[bufferOffset + i][j];
			rgba_colour_t fg = colours[ch.s.fgColour];
			rgba_colour_t bg = colours[ch.s.bgColour];
			Lemon::Graphics::DrawRect(j * 8, i * fontHeight, 8, fontHeight, bg.r, bg.g, bg.b, surface);
			Lemon::Graphics::DrawChar(ch.c, j * 8, i * fontHeight, fg.r, fg.g, fg.b, surface, terminalFont);
		}
	}

	timespec t;
	clock_gettime(CLOCK_BOOTTIME, &t);

	long msec = (t.tv_nsec / 1000000.0);
	if(msec < 250 || (msec > 500 && msec < 750)) // Only draw the cursor for a quarter of a second so it blinks
		Lemon::Graphics::DrawRect(curPos.x * 8, curPos.y * fontHeight + (fontHeight / 4 * 3), 8, fontHeight / 4, colours[0x7] /* Grey */, surface);
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
		if(strchr(escBuf, ';') && *(strchr(escBuf, ';') + 1) == '5' && strchr(strchr(escBuf, ';') + 1, ';')/* Check if 2 arguments are given */){ // Next argument should be '5' for 256 colours
			state.fgColour = atoi(strchr(strchr(escBuf, ';') + 1, ';') + 1); // Get argument
		}
	} else if (r == ANSI_CSI_SGR_BG){
		if(strchr(escBuf, ';') && *(strchr(escBuf, ';') + 1) == '5' && strchr(strchr(escBuf, ';') + 1, ';')/* Check if 2 arguments are given */){ // Next argument should be '5' for 256 colours
			state.bgColour = atoi(strchr(strchr(escBuf, ';') + 1, ';') + 1); // Get argument
		}
	}
}

void DoAnsiCSI(char ch){
	switch(ch){
	case ANSI_CSI_SGR:
		DoAnsiSGR(); // Set Graphics Rendition
		break;
	case ANSI_CSI_CUU:
		{
			int amount = 1;
			if(strlen(escBuf)){
				amount = atoi(escBuf);
			}
			curPos.y -= amount;
			if(curPos.y < 0) curPos.y = 0;
			break;
		}
	case ANSI_CSI_CUD:
		{
			int amount = 1;
			if(strlen(escBuf)){
				amount = atoi(escBuf);
			}
			curPos.y += amount;
			Scroll();
			break;
		}
	case ANSI_CSI_CUF:
		{
			int amount = 1;
			if(strlen(escBuf)){
				amount = atoi(escBuf);
			}
			curPos.x += amount;
			if(curPos.x > columnCount) {
				curPos.x = 0;
				curPos.y++;
				Scroll();
			}
			break;
		}
	case ANSI_CSI_CUB:
		{
			int amount = 1;
			if(strlen(escBuf)){
				amount = atoi(escBuf);
			}
			curPos.x -= amount;
			if(curPos.x < 0) {
				curPos.x = columnCount - 1;
				curPos.y--;
			}
			if(curPos.y < 0) curPos.y = 0;
			break;
		}
	case ANSI_CSI_CUP: // Set cursor position
		{
			char* scolon = strchr(escBuf, ';');
			if(scolon){
				*scolon = 0;

				curPos.y = atoi(escBuf);
				Scroll();

				if(*(scolon + 1) == 0){
					curPos.x = 0;
				} else {
					curPos.x = atoi(scolon + 1);
				}
			}
		}
		break;
	case ANSI_CSI_ED:
		{
			int num = atoi(escBuf);
			switch(num){
				case 0: // Clear entire screen from cursor
					for(int i = curPos.y + 1; i < rowCount && i + bufferOffset < buffer.size(); i++){
						buffer[bufferOffset + i].clear();
					}
					break;
				case 1: // Clear screen and move cursor
				case 2: // Same as 1 but delete everything in the scrollback buffer
					buffer.clear();
					curPos = {0, 0};
					bufferOffset = 0;
					for(int i = 0; i < rowCount; i++){
						buffer.push_back(std::vector<TerminalChar>());
					}
					break;
			}
		}
		break;
	case ANSI_CSI_EL:
		buffer[bufferOffset + curPos.y].erase(buffer[bufferOffset + curPos.y].begin() + curPos.x, buffer[bufferOffset + curPos.y].end());
		break;
	case ANSI_CSI_IL: // Insert blank lines
		{
			int amount = atoi(escBuf);
			buffer.insert(buffer.begin() + bufferOffset + curPos.y, amount, std::vector<TerminalChar>());
			break;
		}
	case ANSI_CSI_DL:
		{
			int amount = atoi(escBuf);
			buffer.erase(buffer.begin() + bufferOffset + curPos.y - amount, buffer.begin() + bufferOffset + curPos.y);

			while(buffer.size() - bufferOffset < rowCount){
				buffer.push_back(std::vector<TerminalChar>());
			}
			break;
		}
	case ANSI_CSI_SU: // Scroll Down
		if(strlen(escBuf)){
			bufferOffset += atoi(escBuf);
		} else {
			bufferOffset++;
		}

		while(buffer.size() - bufferOffset < rowCount){
			buffer.push_back(std::vector<TerminalChar>());
		}
		break;
	case ANSI_CSI_SD: // Scroll up
		if(strlen(escBuf)){
			buffer.insert(buffer.begin() + bufferOffset, atoi(escBuf), std::vector<TerminalChar>());
			bufferOffset -= atoi(escBuf);
		} else {
			buffer.insert(buffer.begin() + bufferOffset, std::vector<TerminalChar>());
			bufferOffset--;
		}
		break;
	default:
		//fprintf(stderr, "Unknown Control Sequence Introducer (CSI) '%c'\n", ch);
		break;
	}
}

void DoAnsiOSC(char ch){

}

void PrintChar(char ch){
	if(escapeSequence){
		if(isspace(ch)){
			return;
		}

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
			buffer.clear();
			curPos = {0, 0};
			bufferOffset = 0;
			for(int i = 0; i < rowCount; i++){
				buffer.push_back(std::vector<TerminalChar>());
			}
		} else if(escapeType == ESC_SAVE_CURSOR) {
			storedCurPos = curPos;	
		} else if(escapeType == ESC_RESTORE_CURSOR) {
			curPos = storedCurPos;	
		} else {
			//fprintf(stderr, "Unknown ANSI escape code '%c'\n", ch);
		}
		escapeSequence = 0;
		escapeType = 0;
	} else {

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
			Scroll();
			break;
		case '\b':
			if(curPos.x > 0) curPos.x--;
			else if(curPos.y > 0) {
				curPos.y--;
				curPos.x = buffer[bufferOffset + curPos.y].size();
			}
			
			buffer[bufferOffset + curPos.y].erase(buffer[bufferOffset + curPos.y].begin() + curPos.x);
			break;
		case ' ':
		default:
			if(!(isgraph(ch) || isspace(ch))) break;

			if(curPos.x > columnCount){
				curPos.y++;
				curPos.x = 0;
				Scroll();
			}

			if(curPos.x >= buffer[bufferOffset + curPos.y].size())
				buffer[bufferOffset + curPos.y].push_back({.s = state, .c = ch});
			else
				buffer[bufferOffset + curPos.y][curPos.x] = {.s = state, .c = ch};

			curPos.x++;

			if(curPos.x > columnCount){
				curPos.x = 0;
				curPos.y++;
				Scroll();
			}
			
			break;
		}
	}
}

extern "C"
int main(char argc, char** argv){
	window = new Lemon::GUI::Window("Terminal", {720, 480});

	terminalFont = Lemon::Graphics::LoadFont("/initrd/sourcecodepro.ttf", "termmonospace");
	if(!terminalFont){
		terminalFont = Lemon::Graphics::GetFont("default");
	}

	rowCount = 480 / terminalFont->height - 1;
	columnCount = 720 / 8;

	curPos = {0, 0};

	for(int i = 0; i < rowCount; i++){
		buffer.push_back(std::vector<TerminalChar>());
	}


	int masterPTYFd;
	syscall(SYS_GRANT_PTY, (uintptr_t)&masterPTYFd, 0, 0, 0, 0);

	setenv("TERM", "xterm-256color", 1); // the Lemon OS terminal is (fairly) xterm compatible (256 colour, etc.)

	char* const _argv[] = {"/system/bin/lsh.lef"};
	lemon_spawn("/system/bin/lsh.lef", 1, _argv, 1);
	
	window->OnPaint = OnPaint;

	char* _buf = (char*)malloc(512);

	bool paint = true;

	winsize wSz = {
		.ws_row = rowCount,
		.ws_col = columnCount,
		.ws_xpixel = window->GetSize().x,
		.ws_ypixel = window->GetSize().y,
	};

	ioctl(masterPTYFd, TIOCSWINSZ, &wSz);

	for(;;){
		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			if(ev.event == Lemon::EventKeyPressed){
				if(ev.key == KEY_ARROW_UP){
					const char* esc = "\e[A";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key == KEY_ARROW_DOWN){
					const char* esc = "\e[B";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key == KEY_ARROW_RIGHT){
					const char* esc = "\e[C";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key == KEY_ARROW_LEFT){
					const char* esc = "\e[D";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key == KEY_END){
					const char* esc = "\e[F";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key == KEY_HOME){
					const char* esc = "\e[H";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key == KEY_ESCAPE){
					const char* esc = "\e\e";
					write(masterPTYFd, esc, strlen(esc));
				} else if(ev.key < 128){
					const char key = (char)ev.key;
					write(masterPTYFd, &key, 1);
				}
			} else if (ev.event == Lemon::EventWindowClosed){
				delete window;
				free(_buf);
				exit(0);
			} else if (ev.event == Lemon::EventWindowResize){
				window->Resize(ev.resizeBounds);

				columnCount = window->GetSize().x / 8;
				rowCount = window->GetSize().y / terminalFont->height;

				wSz.ws_col = columnCount;
				wSz.ws_row = rowCount;
				wSz.ws_xpixel = window->GetSize().x;
				wSz.ws_ypixel = window->GetSize().y;
				
				ioctl(masterPTYFd, TIOCSWINSZ, &wSz);
			}
		}

		while(int len = read(masterPTYFd, _buf, 512)){
			for(int i = 0; i < len; i++){
				PrintChar(_buf[i]);
			}
			paint = true;
		}
		
		//if(paint){
			window->Paint();
			//paint = false;
		//}

		lemon_yield();
	}
	for(;;);
}
