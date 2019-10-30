#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <gfx/window/filedialog.h>
#include <unistd.h>
#include <fcntl.h>

extern "C"
int main(int argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 400;
	windowInfo.height = 300;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	Window* win = CreateWindow(&windowInfo);

	TextBox* box = new TextBox({{0,0},{400,300}});

	char* buffer = (char*)malloc(256);
	int bufferPos = 0;

	AddWidget(box, win);

	PaintWindow(win);

	char* file = FileDialog("/");
	if(file){
		int fd = open(file, 0);
		int size = lseek(fd, 0, SEEK_END);
		if(size > 256){
			buffer = (char*)realloc(buffer, size);
		}
		bufferPos = read(fd, buffer, size);
		close(fd);
	}

	box->LoadText(buffer);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch(msg.data){
				case '\b':
					buffer[--bufferPos] = '\0';
					box->LoadText(buffer);
					break;
				default:
					buffer[bufferPos++] = (char)msg.data;
					box->LoadText(buffer);
					break;
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEUP){
				HandleMouseUp(win);
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;

				HandleMouseDown(win, {mouseX, mouseY});
			} else if (msg.id == WINDOW_EVENT_CLOSE){
				DestroyWindow(win);
				return 0;
			}
		}

		PaintWindow(win);
	}
}