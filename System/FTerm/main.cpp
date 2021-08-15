#include <vector>
#include <cctype>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <lemon/syscall.h>
#include <Lemon/System/Spawn.h>
#include <Lemon/Core/Framebuffer.h>
#include <Lemon/Graphics/Graphics.h>

#include "fterm.h"

#include "colours.h"
#include "escape.h"

struct TermState{
	bool bold : 1 = false;
	bool italic : 1 = false;
	bool faint : 1 = false;
	bool underline : 1 = false;
	bool blink : 1 = false;
	bool reverse : 1 = false;
	bool strikethrough: 1 = false;

	uint8_t fgColour = 15;
	uint8_t bgColour = 0;
};

TermState defaultState {
	.bold = false,
	.italic = false,
	.faint = false,
	.underline = false,
	.blink = false,
	.reverse = false,
	.strikethrough = false,
	.fgColour = 136,
	.bgColour = 0,
};

struct TerminalChar {
	TermState s = defaultState;
	char c = ' ';
};

int masterPTYFd;
TermState state = defaultState;
bool escapeSequence = false;
int escapeType = 0;

Lemon::Graphics::Font* terminalFont;

surface_t fbSurface;
surface_t renderSurface;

winsize wSize;

std::vector<std::vector<TerminalChar>> screenBuffer;

vector2i_t curPos = {0, 0};
vector2i_t storedCurPos = {0, 0};

const int escBufMax = 256;
char escBuf[escBufMax];

void Scroll(){
	while(curPos.y >= wSize.ws_row){
		screenBuffer.erase(screenBuffer.begin());
		screenBuffer.push_back(std::vector<TerminalChar>());

        curPos.y--;
	}

	Lemon::Graphics::DrawRect({0, 0, renderSurface.width, renderSurface.height}, colours[state.bgColour], &renderSurface);
}

void Paint(){
    int lnPos = 0;
    int fontHeight = terminalFont->height;
    for(std::vector<TerminalChar>& line : screenBuffer){
        for(int j = 0; j < static_cast<long>(line.size()) && j < wSize.ws_col; j++){
			TerminalChar ch = screenBuffer[lnPos][j];
			rgba_colour_t fg = colours[ch.s.fgColour];
			rgba_colour_t bg = colours[ch.s.bgColour];

			Lemon::Graphics::DrawRect(j * 8, lnPos * fontHeight, 8, fontHeight, bg.r, bg.g, bg.b, &renderSurface);

			if(isprint(ch.c))
				Lemon::Graphics::DrawChar(ch.c, j * 8, lnPos * fontHeight, fg.r, fg.g, fg.b, &renderSurface, {0, 0, renderSurface.width, renderSurface.height}, terminalFont);
        }

        if(++lnPos >= wSize.ws_row){
            break;
        }
    }

	timespec t;
	clock_gettime(CLOCK_BOOTTIME, &t);
	
	long msec = (t.tv_nsec / 1000000.0);
	if(msec < 250 || (msec > 500 && msec < 750)) // Only draw the cursor for a quarter of a second so it blinks
		Lemon::Graphics::DrawRect(curPos.x * 8, curPos.y * fontHeight + (fontHeight / 4 * 3), 8, fontHeight / 4, colours[state.fgColour], &renderSurface);

    Lemon::Graphics::surfacecpy(&fbSurface, &renderSurface);
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
			if(curPos.x > wSize.ws_col) {
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
				curPos.x = wSize.ws_col - 1;
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
					for(int i = curPos.y + 1; i < wSize.ws_row && i < static_cast<long>(screenBuffer.size()); i++){
						screenBuffer[i].clear();
					}
					break;
				case 1: // Clear screen and move cursor
				case 2: // Same as 1 but delete everything in the scrollback buffer
					screenBuffer.clear();
					curPos = {0, 0};
					for(int i = 0; i < wSize.ws_row; i++){
						screenBuffer.push_back(std::vector<TerminalChar>());
					}
					break;
			}
		}
		break;
	case ANSI_CSI_EL:
		screenBuffer[curPos.y].erase(screenBuffer[curPos.y].begin() + curPos.x, screenBuffer[curPos.y].end());
		break;
	case ANSI_CSI_IL: // Insert blank lines
		{
			int amount = atoi(escBuf);
			screenBuffer.insert(screenBuffer.begin() + curPos.y, amount, std::vector<TerminalChar>());
			break;
		}
	case ANSI_CSI_DL:
		{
			int amount = atoi(escBuf);
			screenBuffer.erase(screenBuffer.begin() + curPos.y - amount, screenBuffer.begin() + curPos.y);

			while(screenBuffer.size() < wSize.ws_row){
				screenBuffer.push_back(std::vector<TerminalChar>());
			}
			break;
		}
	case ANSI_CSI_SU: // Scroll Down
        {
            int amount = 1;
            if(strlen(escBuf)){
                amount = atoi(escBuf);
            }

            while(amount--){
                screenBuffer.erase(screenBuffer.begin());
                screenBuffer.push_back(std::vector<TerminalChar>());
            }

            break;
        }
	case ANSI_CSI_SD: // Scroll up
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
			screenBuffer.clear();
			curPos = {0, 0};
			for(int i = 0; i < wSize.ws_row; i++){
				screenBuffer.push_back(std::vector<TerminalChar>());
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
			if(curPos.x > 0) {
				curPos.x--;
			} else if(curPos.y > 0) {
				curPos.y--;
				curPos.x = screenBuffer[curPos.y].size();
			}
			
			if(curPos.x < static_cast<long>(screenBuffer[curPos.y].size()))
				screenBuffer[curPos.y].erase(screenBuffer[curPos.y].begin() + curPos.x);

			break;
		case ' ':
		default:
			if(!(isgraph(ch) || isspace(ch))) break;

			if(curPos.x > wSize.ws_col){
				curPos.y++;
				curPos.x = 0;
				Scroll();
			}

			if(curPos.x >= static_cast<long>(screenBuffer[curPos.y].size()))
				screenBuffer[curPos.y].push_back({.s = state, .c = ch});
			else
				screenBuffer[curPos.y][curPos.x] = {.s = state, .c = ch};

			curPos.x++;

			if(curPos.x > wSize.ws_col){
				curPos.x = 0;
				curPos.y++;
				Scroll();
			}
			
			break;
		}
	}
}

void OnKey(int key){
    if(key == KEY_ARROW_UP){
        const char* esc = "\e[A";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key == KEY_ARROW_DOWN){
        const char* esc = "\e[B";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key == KEY_ARROW_RIGHT){
        const char* esc = "\e[C";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key == KEY_ARROW_LEFT){
        const char* esc = "\e[D";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key == KEY_END){
        const char* esc = "\e[F";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key == KEY_HOME){
        const char* esc = "\e[H";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key == KEY_ESCAPE){
        const char* esc = "\e\e";
        write(masterPTYFd, esc, strlen(esc));
    } else if(key < 128){
        const char tkey = (char)key;
        write(masterPTYFd, &tkey, 1);
    }
}

int main(int argc, char** argv){
    Lemon::CreateFramebufferSurface(fbSurface);
    renderSurface = fbSurface;
    renderSurface.buffer = new uint8_t[fbSurface.width * fbSurface.height * 4];

    wSize.ws_xpixel = renderSurface.width;
    wSize.ws_ypixel = renderSurface.height;
    
	terminalFont = Lemon::Graphics::LoadFont("/initrd/sourcecodepro.ttf", "termmonospace");
	if(!terminalFont){
		terminalFont = Lemon::Graphics::GetFont("default");
	}

    wSize.ws_col = renderSurface.width / 8;
    wSize.ws_row = renderSurface.height / terminalFont->height;

    InputManager input;

    for(int i = 0; i < wSize.ws_row; i++){
        screenBuffer.push_back(std::vector<TerminalChar>(wSize.ws_col));
    }

	syscall(SYS_GRANT_PTY, (uintptr_t)&masterPTYFd, 0, 0, 0, 0);

	setenv("TERM", "xterm-256color", 1);
	ioctl(masterPTYFd, TIOCSWINSZ, &wSize);

    char* const args[] = {"/initrd/lsh.lef"};
    lemon_spawn(*args, 1, args, 1);

    char _buf[512];
	bool paint = true;

    for(;;){
        input.Poll();

		while(int len = read(masterPTYFd, _buf, 512)){
			for(int i = 0; i < len; i++){
				PrintChar(_buf[i]);
			}
			paint = true;
		}

		if(paint){
        	Paint();
		}
    }
}