#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>

extern "C"
int main(int argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 400;
	windowInfo.height = 300;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	Window* win = CreateWindow(&windowInfo);

	Bitmap* drawSpace = new Bitmap({{100,0},{300,300}});
	DrawRect(0,0,300,300,255,255,255,&drawSpace->surface);
	AddWidget(drawSpace, win);

	bool penDown;
	int penSize = 2;

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.id == WINDOW_EVENT_MOUSEUP){
				penDown = false;
				HandleMouseUp(win);
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;
				if(mouseX > 100){
					penDown = true;
					DrawRect(mouseX - 100, mouseY, penSize, penSize, 0, 0, 0, &drawSpace->surface);
				} else HandleMouseDown(win, {mouseX, mouseY});
			} else if (msg.id == WINDOW_EVENT_CLOSE){
				DestroyWindow(win);
				exit();
				for(;;);
			} else if (msg.id == WINDOW_EVENT_KEY){
				switch ((char)msg.data){
					case '1':
						penSize = 1;
						break;
					case '2':
						penSize = 2;
						break;
					case '3':
						penSize = 5;
						break;
					case '4':
						penSize = 8;
						break;
				}
			}
		}

		PaintWindow(win);
	}
}