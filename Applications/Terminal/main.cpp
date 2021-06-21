#include <Lemon/Graphics/Surface.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/Core/Keyboard.h>

#include <lemon/syscall.h>
#include <Lemon/System/Filesystem.h>
#include <Lemon/System/Spawn.h>
#include <Lemon/System/Util.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <vector>

#include "escape.h"
#include "colours.h"

#define TASKBAR_COLOUR_R 128
#define TASKBAR_COLOUR_G 128
#define TASKBAR_COLOUR_B 128

#define MENU_WIDTH 200
#define MENU_HEIGHT 300

Lemon::GUI::Window* window;

bool paint = true;
bool paintAll = true;

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
	.bold = 0,
	.italic = 0,
	.faint = 0,
	.underline = 0,
	.blink = 0,
	.reverse = 0,
	.strikethrough = 0,
	.fgColour = 15,
	.bgColour = 0,
};

struct TerminalChar {
	TermState s;
	char c;
};

rgba_colour_t* colours = coloursProfile2;

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

char charactersPerLine; // Characters displayed per line

int masterPTYFd; // PTY file desc
pid_t lsh; // LSh Process PID

void Scroll(){
	if(curPos.y >= rowCount){
		bufferOffset += curPos.y - (rowCount - 1);
		curPos.y = rowCount - 1;
	}

	while(static_cast<std::size_t>(bufferOffset + rowCount) >= buffer.size()) buffer.push_back(std::vector<TerminalChar>());
			
	if(buffer.size() > 2000){
		buffer.erase(buffer.begin() + 2000, buffer.end());
	}
	
	paintAll = true;
}

void OnPaint(surface_t* surface){
	int fontHeight = terminalFont->lineHeight;

	paintAll = true;

	if(paintAll){
		for(int i = 0; i < rowCount && (bufferOffset + i) < static_cast<int>(buffer.size()); i++){
			int j = 0;
			for(; j < static_cast<int>(buffer[bufferOffset + i].size()); j++){
				TerminalChar ch = buffer[bufferOffset + i][j];
				rgba_colour_t fg = colours[ch.s.fgColour];
				rgba_colour_t bg = colours[ch.s.bgColour];
				Lemon::Graphics::DrawRect(j * 8, i * fontHeight, 8, fontHeight, bg.r, bg.g, bg.b, surface);
				Lemon::Graphics::DrawChar(ch.c, j * 8, i * fontHeight, fg.r, fg.g, fg.b, surface, terminalFont);
			}
			
			Lemon::Graphics::DrawRect(j * 8, i * fontHeight, window->GetSize().x - j * 8, fontHeight, colours[state.bgColour], surface);
		}
	} else if(static_cast<unsigned>(curPos.y) < buffer.size()) {
		int j = 0;
		for(; j < static_cast<int>(buffer[bufferOffset + curPos.y].size()); j++){
			TerminalChar ch = buffer[bufferOffset + curPos.y][j];
			rgba_colour_t fg = colours[ch.s.fgColour];
			rgba_colour_t bg = colours[ch.s.bgColour];
			Lemon::Graphics::DrawRect(j * 8, curPos.y * fontHeight, 8, fontHeight, bg.r, bg.g, bg.b, surface);
			Lemon::Graphics::DrawChar(ch.c, j * 8, curPos.y * fontHeight, fg.r, fg.g, fg.b, surface, terminalFont);
		}

		Lemon::Graphics::DrawRect(j * 8, curPos.y * fontHeight, window->GetSize().x -  j * 8, fontHeight, colours[state.bgColour], surface);
	}

	Lemon::Graphics::DrawRect(curPos.x * 8, curPos.y * fontHeight + (fontHeight / 4 * 3), 8, fontHeight / 4, colours[0x7] /* Grey */, surface);

	paintAll = false;
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

			paintAll = true;
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

			paintAll = true;
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

				paintAll = true;
			}
			if(curPos.y < 0) curPos.y = 0;
			break;
		}
	case ANSI_CSI_CUP: // Set cursor position
		{
			char* scolon = strchr(escBuf, ';');
			if(scolon){
				*scolon = 0;

				curPos.y = atoi(escBuf) - 1;
				Scroll();

				if(*(scolon + 1) == 0){
					curPos.x = 0;
				} else {
					curPos.x = atoi(scolon + 1) - 1;
				}
			} else {
				curPos.x = 0;
				curPos.y = 0;
			}

			paintAll = true;
		}
		break;
	case ANSI_CSI_ED:
		{
			int num = atoi(escBuf);
			switch(num){
				case 0: // Clear entire screen from cursor
					for(int i = curPos.y + 1; i < rowCount && i + bufferOffset < static_cast<int>(buffer.size()); i++){
						buffer[bufferOffset + i].clear();
					}
					paintAll = true;
					break;
				case 1: // Clear screen and move cursor
				case 2: // Same as 1 but delete everything in the scrollback buffer
					buffer.clear();
					curPos = {0, 0};
					bufferOffset = 0;
					for(int i = 0; i < rowCount; i++){
						buffer.push_back(std::vector<TerminalChar>());
					}
					paintAll = true;
					break;
			}
		}
		break;
	case ANSI_CSI_EL:
		{
			int n = 0;
			if(strlen(escBuf)){
				n = atoi(escBuf);
			}

			switch (n)
			{
			case 2: // Clear entire screen
				curPos.y = 0;
				curPos.x = 0;

				bufferOffset = 0;
				buffer.clear();
				paintAll = true;
				break;
			case 1: // Clear from cursor to beginning of line
				if(bufferOffset + curPos.y < static_cast<int>(buffer.size())){
					if(curPos.x < static_cast<int>(buffer[bufferOffset + curPos.y].size())){
						buffer[bufferOffset + curPos.y].erase(buffer[bufferOffset + curPos.y].begin(), buffer[bufferOffset + curPos.y].begin() + curPos.x);
					} else {
						buffer[bufferOffset + curPos.y].clear();
					}
					curPos.x = 0;
				}
				break;
			case 0: // Clear from cursor to end of line
			default:
				buffer[bufferOffset + curPos.y].erase(buffer[bufferOffset + curPos.y].begin() + curPos.x, buffer[bufferOffset + curPos.y].end());
				break;
			}
			break;
		}
	case ANSI_CSI_IL: // Insert blank lines
		{
			int amount = atoi(escBuf);
			buffer.insert(buffer.begin() + bufferOffset + curPos.y, amount, std::vector<TerminalChar>());

			paintAll = true;
			break;
		}
	case ANSI_CSI_DL:
		{
			int amount = atoi(escBuf);
			buffer.erase(buffer.begin() + bufferOffset + curPos.y - amount, buffer.begin() + bufferOffset + curPos.y);

			while(static_cast<int>(buffer.size()) - bufferOffset < rowCount){
				buffer.push_back(std::vector<TerminalChar>());
			}

			paintAll = true;
			break;
		}
	case ANSI_CSI_SU: // Scroll Up
		if(strlen(escBuf)){
			bufferOffset += atoi(escBuf);
		} else {
			bufferOffset++;
		}

		while(static_cast<int>(buffer.size()) - bufferOffset < rowCount){
			buffer.push_back(std::vector<TerminalChar>());
		}

		paintAll = true;
		break;
	case ANSI_CSI_SD: // Scroll Down
		if(strlen(escBuf)){
			buffer.insert(buffer.begin() + bufferOffset, atoi(escBuf), std::vector<TerminalChar>());
			//bufferOffset -= atoi(escBuf);

			paintAll = true;
		} else {
			buffer.insert(buffer.begin() + bufferOffset, 1, std::vector<TerminalChar>());
			//bufferOffset--;

			paintAll = true;
		}
		break;
	default:
		//fprintf(stderr, "Unknown Control Sbequence Introducer (CSI) '%c'\n", ch);
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
		case '\r':
			curPos.x = 0;
			break;
		case '\n':
			curPos.y++;
			curPos.x = 0;
			Scroll();

			paintAll = true;
			break;
		case '\b':
			if(curPos.x > 0) curPos.x--;
			else if(curPos.y > 0) {
				curPos.y--;
				curPos.x = buffer[bufferOffset + curPos.y].size();

				paintAll = true;
			} else {
				break;
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

			if(static_cast<std::size_t>(curPos.x) >= buffer[bufferOffset + curPos.y].size())
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

[[noreturn]] void* PTYThread(void*){
	std::vector<pollfd> fds;
	fds.push_back({.fd = masterPTYFd, .events = POLLIN, .revents = 0});

	char buf[512];

	while(waitpid(lsh, nullptr, WNOHANG) <= 0){
		poll(fds.data(), fds.size(), 500000); // Wake up every 500ms to check if LSh has exited

		while(int len = read(masterPTYFd, buf, 512)){
			for(int i = 0; i < len; i++){
				PrintChar(buf[i]);
			}

			paint = true;
		}

		if(paint){
			Lemon::InterruptThread(0);
		}
	}

	exit(0); // LSh must have exited
}

int main(int argc, char** argv){
	terminalFont = Lemon::Graphics::LoadFont("/system/lemon/resources/fonts/sourcecodepro.ttf", "termmonospace");
	if(!terminalFont){
		terminalFont = Lemon::Graphics::GetFont("default");
	}
	
	window = new Lemon::GUI::Window("Terminal", {720, 32 * terminalFont->lineHeight});

	rowCount = window->GetSize().y / terminalFont->lineHeight;
	columnCount = 720 / 8;

	curPos = {0, 0};

	for(int i = 0; i < rowCount; i++){
		buffer.push_back(std::vector<TerminalChar>());
	}

	syscall(SYS_GRANT_PTY, (uintptr_t)&masterPTYFd, 0, 0, 0, 0);
	setenv("TERM", "xterm-256color", 1); // the Lemon OS terminal is (fairly) xterm compatible (256 colour, etc.)

	char* const _argv[] = {const_cast<char*>("/system/bin/lsh.lef")};
	lsh = lemon_spawn(_argv[0], 1, _argv, 1);

	if(lsh < 0){
		perror("Error running lsh");
		return 1;
	}
	
	window->OnPaint = OnPaint;

	winsize wSz = {
		.ws_row = static_cast<unsigned short>(rowCount),
		.ws_col = static_cast<unsigned short>(columnCount),
		.ws_xpixel = static_cast<unsigned short>(window->GetSize().x),
		.ws_ypixel = static_cast<unsigned short>(window->GetSize().y),
	};

	ioctl(masterPTYFd, TIOCSWINSZ, &wSz);

	pthread_t ptyThread;
	if(pthread_create(&ptyThread, nullptr, PTYThread, nullptr)){
		perror("Error creating PTY thread");
		return 2;
	}

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
				exit(0);
				return 0;
			} else if (ev.event == Lemon::EventWindowResize){
				window->Resize(ev.resizeBounds);

				columnCount = window->GetSize().x / 8;
				rowCount = window->GetSize().y / terminalFont->lineHeight;

				wSz.ws_col = static_cast<unsigned short>(columnCount);
				wSz.ws_row = static_cast<unsigned short>(rowCount);
				wSz.ws_xpixel = static_cast<unsigned short>(window->GetSize().x);
				wSz.ws_ypixel = static_cast<unsigned short>(window->GetSize().y);
				
				ioctl(masterPTYFd, TIOCSWINSZ, &wSz);
				paintAll = true;
			}
		}

		if(paint || paintAll){
			window->Paint();

			paint = false;
			paintAll = false;
		}

		window->WaitEvent();
	}

	return 0;
}
