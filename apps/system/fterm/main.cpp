#include <cassert>
#include <cstdlib>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>

#include <Lemon/Core/Keyboard.h>
#include <Lemon/Core/Framebuffer.h>

#include <Lemon/Graphics/Graphics.h>

#include "colours.h"
#include "escape.h"
#include "fterm.h"

#define SCROLLBACK_BUFFER_MAX 400

using namespace Lemon;

Colour* colours = coloursProfile2;
Lemon::Graphics::Font* terminalFont = nullptr;

Surface renderSurface;
Surface framebufferSurface;

RGBAColour defaultBackgroundColour = colours[0];
RGBAColour defaultForegroundColour = colours[15];
RGBAColour currentBackgroundColour = colours[0];
RGBAColour currentForegroundColour = colours[15];

Vector2i characterSize;
Vector2i terminalSize;            // The size of the terminal window in characters
Vector2i cursorPosition = {1, 1}; // The position of the cursor in characters

pid_t shellPID = -1;

struct TerminalChar {
    char ch;
    RGBAColour background;
    RGBAColour foreground;

    TerminalChar() = default;
    TerminalChar(char ch) : ch(ch), background(currentBackgroundColour), foreground(currentForegroundColour) {}

    inline operator char() { return ch; }
};

using TerminalLine = std::vector<TerminalChar>;

std::vector<TerminalLine> scrollbackBuffer;

TerminalLine& GetLine(int cursorY) {
    assert(scrollbackBuffer.size() >= static_cast<unsigned>(terminalSize.y));

    // Cursor starts at (1, 1)
    TerminalLine& ln = scrollbackBuffer[scrollbackBuffer.size() - terminalSize.y + (cursorY - 1)];

    // Ensure the line is the right size
    if (ln.size() != static_cast<unsigned>(terminalSize.x))
        ln.resize(terminalSize.x);
    return ln;
}

inline auto FirstLine() { return scrollbackBuffer.begin() + (scrollbackBuffer.size() - terminalSize.y); }

void Paint() {
    assert(scrollbackBuffer.size() >= static_cast<unsigned>(terminalSize.y));

    Vector2i screenPos = {0, 0};
    for (unsigned i = scrollbackBuffer.size() - terminalSize.y; i < scrollbackBuffer.size(); i++) {
        screenPos.x = 0;
        for (auto& c : scrollbackBuffer[i]) {
            Lemon::Graphics::DrawRect(Rect{screenPos, characterSize}, c.background, &renderSurface);
            Lemon::Graphics::DrawChar(c.ch, screenPos.x, screenPos.y, c.foreground, &renderSurface, terminalFont);
            screenPos.x += characterSize.x;
        }
        screenPos.y += characterSize.y;
    }

    Lemon::Graphics::DrawRect(
        Rect{{(cursorPosition.x - 1) * characterSize.x, (cursorPosition.y - 1) * characterSize.y}, characterSize},
        currentForegroundColour, &renderSurface);
	
	framebufferSurface.Blit(&renderSurface);
}

void ClearScrollbackBuffer() {
    scrollbackBuffer = std::vector<TerminalLine>(terminalSize.y, TerminalLine(terminalSize.x, TerminalChar(0)));
}

void AddLine() {
    if (scrollbackBuffer.size() < SCROLLBACK_BUFFER_MAX) {
        scrollbackBuffer.push_back(TerminalLine(terminalSize.x, TerminalChar(0)));
    } else {
        scrollbackBuffer.erase(scrollbackBuffer.begin());
        scrollbackBuffer.push_back(TerminalLine(terminalSize.x, TerminalChar(0)));
    }
}

void AdvanceCursorY() {
    cursorPosition.y++;
    while (cursorPosition.y > terminalSize.y) {
        AddLine();
        cursorPosition.y--;
    }
}

void AdvanceCursorX() {
    if (cursorPosition.x++ > terminalSize.x) {
        cursorPosition.x = 1;
        AdvanceCursorY();
    }
}

// Place char at the cursor position
void PutChar(char ch) {
    assert(isprint(ch));

    if (cursorPosition.x > terminalSize.x) {
        cursorPosition.x = 1;
        AdvanceCursorY();
    }

    TerminalLine& line = GetLine(cursorPosition.y);
    line.at(cursorPosition.x - 1) = TerminalChar(ch);
}

// Print a char on screen and advance the cursor
void PrintChar(char ch) {
    PutChar(ch);
    AdvanceCursorX();
}

enum TerminalParseState {
    State_Normal,
    State_Escape,
    State_CSI,
    State_OSC,
    State_SGR,
};
TerminalParseState parseState = State_Normal;
std::string escapeBuffer = "";

Vector2i savedCursorPosition; // Cursor for save/restore

// Parse input character
void ParseChar(char ch) {
    if (parseState == State_Normal) {
        if (isprint(ch) || ch == ' ') {
            PrintChar(ch);
        } else if (ch == Control_Escape) {
            escapeBuffer.clear();
            parseState = State_Escape;
        } else if (ch == Control_Backspace) {
            if (cursorPosition.x > 1) {
                cursorPosition.x--;
                PutChar(' ');
            } else {
                PutChar(' ');
            }
        } else if (ch == Control_LineFeed) {
            AdvanceCursorY();
            cursorPosition.x = 1;
        } else if (ch == Control_CarriageReturn) {
            cursorPosition.x = 1;
        }
    } else if (parseState == State_Escape) {
        switch (ch) {
        case ANSI_RIS: // Reset to initial state
            parseState = State_Normal;
            ClearScrollbackBuffer();
            cursorPosition = {1, 1};
            break;
        case ANSI_CSI:
            parseState = State_CSI;
            break;
        case ANSI_OSC:
            parseState = State_OSC;
            break;
        case ESC_SAVE_CURSOR:
            savedCursorPosition = cursorPosition;
            parseState = State_Normal;
            break;
        case ESC_RESTORE_CURSOR:
            cursorPosition = savedCursorPosition;
            parseState = State_Normal;
            break;
        default: // Unsupported escape sequence
            parseState = State_Normal;
            break;
        }

        assert(parseState != State_Escape); // Make sure the statement changed the parser state
    } else if (parseState == State_CSI) {
        if (!isalpha(ch)) { // If it is not a letter, add to the escape buffer
            escapeBuffer += ch;
        } else {
            switch (ch) {
            case ANSI_CSI_SGR:
                parseState = State_SGR;
                break;
            case ANSI_CSI_CUU: { // Cursor up
                int amount = 1;
                if (escapeBuffer.size()) {
                    amount = std::stoi(escapeBuffer);
                }

                cursorPosition.y = std::max(cursorPosition.y - amount, 1);
                break;
            }
            case ANSI_CSI_CUD: { // Cursor down
                int amount = 1;
                if (escapeBuffer.size()) {
                    amount = std::stoi(escapeBuffer);
                }

                cursorPosition.y = std::min(cursorPosition.y + amount, terminalSize.y);
                break;
            }
            case ANSI_CSI_CUF: { // Cursor forward
                int amount = 1;
                if (escapeBuffer.size()) {
                    amount = std::stoi(escapeBuffer);
                }

                cursorPosition.x = std::min(cursorPosition.x + amount, terminalSize.x);
                break;
            }
            case ANSI_CSI_CUB: { // Cursor back
                int amount = 1;
                if (escapeBuffer.size()) {
                    amount = std::stoi(escapeBuffer);
                }

                cursorPosition.x = std::max(cursorPosition.x - amount, 1);
                break;
            }
            case ANSI_CSI_CUP: // Set cursor position
            {
                cursorPosition.x = 1;
                cursorPosition.y = 1;

                size_t semicolon = escapeBuffer.find(';');
                if (semicolon != std::string::npos) {
                    // Use atoi to prevent exceptions
                    // 0 on error works well as the cursor position cannot be 0
                    cursorPosition.y =
                        std::min(std::max(atoi(escapeBuffer.substr(0, semicolon).c_str()), 1), terminalSize.y);

                    std::string afterSemicolon = escapeBuffer.substr(semicolon + 1);
                    if (afterSemicolon.length()) {
                        cursorPosition.x = std::min(std::max(atoi(afterSemicolon.c_str()), 1), terminalSize.x);
                    }
                }
            } break;
            case ANSI_CSI_ED: {
                int num = atoi(escapeBuffer.c_str());
                auto& ln = GetLine(cursorPosition.y);
                switch (num) {
                default:
                case 0: // Clear entire screen from cursor
                    ln.erase(ln.begin() + (cursorPosition.x - 1), ln.end());
                    ln.insert(ln.end(), terminalSize.x - (cursorPosition.x - 1), TerminalChar(0));

                    assert(ln.size() == static_cast<unsigned>(terminalSize.x));
                    scrollbackBuffer.erase(FirstLine() + (cursorPosition.y - 1) + 1, scrollbackBuffer.end());
                    scrollbackBuffer.insert(scrollbackBuffer.end(), terminalSize.y - (cursorPosition.y - 1),
                                            TerminalLine(terminalSize.x, TerminalChar(0)));
                    break;
                case 1: // Clear screen and move cursor
                case 2: // Same as 1 but delete everything in the scrollback buffer
                    ClearScrollbackBuffer();
                    cursorPosition = {1, 1};
                    break;
                }
                break;
            }
            case ANSI_CSI_EL: {
                int n = 0;
                if (escapeBuffer.length()) {
                    n = atoi(escapeBuffer.c_str());
                }

                auto& ln = GetLine(cursorPosition.y);
                switch (n) {
                case 2: // Clear entire screen
                    ClearScrollbackBuffer();
                    cursorPosition = {1, 1};
                    break;
                case 1: // Clear from cursor to beginning of line
                    ln.erase(ln.begin(), ln.begin() + cursorPosition.x);
                    ln.insert(ln.begin(), cursorPosition.x, TerminalChar(0));
                    break;
                case 0: // Clear from cursor to end of line
                default:
                    ln.erase(ln.begin() + (cursorPosition.x - 1), ln.end());
                    ln.insert(ln.end(), terminalSize.x - (cursorPosition.x - 1), TerminalChar(0));
                    break;
                }
                break;
            }
            case ANSI_CSI_IL: // Insert blank lines
            {
                // Unsupported
                break;
            }
            case ANSI_CSI_DL: {
                // Unsupported
                break;
            }
            case ANSI_CSI_SU: // Scroll Up
            {
                int amount = 1;
                if (strlen(escapeBuffer.c_str())) {
                    amount = atoi(escapeBuffer.c_str());
                }

                scrollbackBuffer.insert(scrollbackBuffer.end(), amount, TerminalLine(terminalSize.x, TerminalChar(0)));
                break;
            }
            case ANSI_CSI_SD: { // Scroll Down
                int amount = 1;
                if (strlen(escapeBuffer.c_str())) {
                    amount = atoi(escapeBuffer.c_str());
                }

                scrollbackBuffer.insert(FirstLine(), amount, TerminalLine(terminalSize.x, TerminalChar(0)));
                break;
            }
            default:
                break;
            }

            if (parseState == State_CSI) {
                parseState = State_Normal;
            }
        }
    } else if (parseState == State_OSC) {
        parseState = State_Normal;
    }

    if (parseState == State_SGR) {
        size_t semicolon = escapeBuffer.find(';');
        int r = 0;

        if (semicolon != std::string::npos) {
            r = atoi(escapeBuffer.substr(0, semicolon).c_str());
        } else {
            r = atoi(escapeBuffer.c_str());
        }

        if (r == 0) {
            currentBackgroundColour = defaultBackgroundColour;
            currentForegroundColour = defaultForegroundColour;
        } else if (r >= ANSI_CSI_SGR_FG_BLACK && r <= ANSI_CSI_SGR_FG_WHITE) { // Foreground Colour
            currentForegroundColour = colours[r - ANSI_CSI_SGR_FG_BLACK];
        } else if (r >= ANSI_CSI_SGR_BG_BLACK && r <= ANSI_CSI_SGR_BG_WHITE) { // Background Colour
            currentBackgroundColour = colours[r - ANSI_CSI_SGR_BG_BLACK];
        } else if (r >= ANSI_CSI_SGR_FG_BLACK_BRIGHT &&
                   r <= ANSI_CSI_SGR_FG_WHITE_BRIGHT) { // Foreground Colour (Bright)
            currentForegroundColour = colours[r - ANSI_CSI_SGR_FG_BLACK_BRIGHT + 8];
        } else if (r >= ANSI_CSI_SGR_BG_BLACK_BRIGHT &&
                   r <= ANSI_CSI_SGR_BG_WHITE_BRIGHT) { // Background Colour (Bright)
            currentBackgroundColour = colours[r - ANSI_CSI_SGR_BG_BLACK_BRIGHT + 8];
        } else if (r == ANSI_CSI_SGR_FG) {
            size_t firstSemicolon = escapeBuffer.find_first_of(';');
            size_t secondSemicolon = escapeBuffer.find(';', firstSemicolon + 1);
            // Next argument should be '5' for 256 colours
            // e.g. [38;5;<colour number>
            if (firstSemicolon != std::string::npos && escapeBuffer[firstSemicolon + 1] == '5' &&
                secondSemicolon != std::string::npos) {
                int col = atoi(escapeBuffer.substr(secondSemicolon + 1).c_str());
                if (col < 256) {
                    currentForegroundColour = colours[col];
                }
            }
        } else if (r == ANSI_CSI_SGR_BG) {
            size_t secondSemicolon = escapeBuffer.find(';', semicolon + 1);
            // Next argument should be '5' for 256 colours
            // e.g. [48;5;<colour number>
            if (semicolon != std::string::npos && escapeBuffer[semicolon + 1] == '5' &&
                secondSemicolon != std::string::npos) {
                int col = atoi(escapeBuffer.substr(secondSemicolon + 1).c_str());
                if (col < 256) {
                    currentBackgroundColour = colours[col];
                }
            }
        }

        parseState = State_Normal;
    }
}

bool shouldPaint = true;
std::mutex paintMutex;
std::mutex bufferMutex;

bool isOpen = true;
int ptyMasterFd = -1;

void SIGCHLDHandler(int){
    isOpen = false; // Shell has closed
}

void* PTYThread() {
    pollfd pollFd = {.fd = ptyMasterFd, .events = POLLIN};
    while (isOpen) {
        if (poll(&pollFd, 1, 500000) > 0) {
            char buf[512];
            ssize_t r;
            bufferMutex.lock();
            while ((r = read(ptyMasterFd, buf, 512)) > 0) {
                int i = 0;
                while (r--) {
                    ParseChar(buf[i++]);
                }
            }
            bufferMutex.unlock();

            std::unique_lock lock(paintMutex);
			Paint();
            shouldPaint = false;
        }
    }

    return nullptr;
}

void OnKey(int key) {
    if (key == KEY_ARROW_UP) {
        const char* esc = "\e[A";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key == KEY_ARROW_DOWN) {
        const char* esc = "\e[B";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key == KEY_ARROW_RIGHT) {
        const char* esc = "\e[C";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key == KEY_ARROW_LEFT) {
        const char* esc = "\e[D";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key == KEY_END) {
        const char* esc = "\e[F";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key == KEY_HOME) {
        const char* esc = "\e[H";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key == KEY_ESCAPE) {
        const char* esc = "\e\e";
        write(ptyMasterFd, esc, strlen(esc));
    } else if (key < 128) {
        const char tkey = (char)key;
        write(ptyMasterFd, &tkey, 1);
    }
}

int main(int argc, char** argv) {
    Lemon::CreateFramebufferSurface(framebufferSurface);
    renderSurface = framebufferSurface;
    renderSurface.buffer = new uint8_t[framebufferSurface.width * framebufferSurface.height * 4];

    terminalFont = Lemon::Graphics::LoadFont("/initrd/sourcecodepro.ttf", "termmonospace");
    if (!terminalFont) {
        terminalFont = Lemon::Graphics::GetFont("default");
    }
	
	characterSize = Vector2i{terminalFont->width, terminalFont->lineHeight};
    terminalSize =
        Vector2i{renderSurface.width / characterSize.x, renderSurface.height / characterSize.y};

    ClearScrollbackBuffer();
    assert(FirstLine() == scrollbackBuffer.begin());
    assert(&GetLine(1) == &scrollbackBuffer.at(0));

    int ptySlaveFd = -1;

    setenv("TERM", "xterm-256color", 1);

    struct sigaction action = {};
    action.sa_handler = SIGCHLDHandler;
    
    sigemptyset(&action.sa_mask);

    // We want to exit on SIGCHLD
    if(sigaction(SIGCHLD, &action, nullptr)){
        perror("sigaction");
        return 99;
    }

    if (openpty(&ptyMasterFd, &ptySlaveFd, nullptr, nullptr, nullptr)) {
        perror("openpty");
        return 2;
    }

    winsize wSz = {
        .ws_row = static_cast<unsigned short>(terminalSize.y),
        .ws_col = static_cast<unsigned short>(terminalSize.x),
        .ws_xpixel = static_cast<unsigned short>(framebufferSurface.width),
        .ws_ypixel = static_cast<unsigned short>(framebufferSurface.height),
    };

    InputManager input;

    ioctl(ptyMasterFd, TIOCSWINSZ, &wSz);

	for(;;) {
		// No need to use Lemon OS specifics here
		shellPID = fork();
		if (shellPID == 0) {
			if (dup2(ptySlaveFd, STDIN_FILENO) < 0) {
				perror("dup2");
				exit(3);
			}

			if (dup2(ptySlaveFd, STDOUT_FILENO) < 0) {
				perror("dup2");
				exit(3);
			}

			if (dup2(ptySlaveFd, STDERR_FILENO) < 0) {
				perror("dup2");
				exit(3);
			}

			close(ptySlaveFd);

			char arg0[] = "/bin/lsh";
			char* const shArgv[] {
				arg0,
				nullptr,
			};
			if (execve(arg0, shArgv, environ)) {
				perror("execve");
				exit(1);
			}
		} else if (shellPID < 0) {
			perror("fork");
			return 1;
		}

		std::thread ptyThread(PTYThread);
		while (isOpen) {
			input.Poll();
		}
	}
}