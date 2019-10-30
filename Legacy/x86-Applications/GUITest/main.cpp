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

#include <gfx/window/messagebox.h>

extern "C"
int main(int argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 320;
	windowInfo.height = 200;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	Window* win = CreateWindow(&windowInfo);

	Button* btn1 = new Button("Button 1", {10,10,100,24});
	Button* btn2 = new Button("Button 2", {10,36,100,24});
	Button* btn3 = new Button("Button 3", {10,62,100,24});
	Button* btn4 = new Button("Button 4", {10,88,100,24});

	btn2->style = 1;
	btn3->style = 2;
	btn4->style = 3;

	AddWidget(btn1, win);
	AddWidget(btn2, win);
	AddWidget(btn3, win);
	AddWidget(btn4, win);

	MessageBox("Nice", MESSAGEBOX_OK);
	MessageBox("Even Nicer", MESSAGEBOX_OKCANCEL);
	MessageBox("Sehr Gut", MESSAGEBOX_EXITRETRYIGNORE);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.id == WINDOW_EVENT_MOUSEUP){
				HandleMouseUp(win);
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;

				HandleMouseDown(win, {mouseX, mouseY});
			} else if (msg.id == WINDOW_EVENT_CLOSE){
				DestroyWindow(win);
				exit();
			}
		}

		PaintWindow(win);
	}
}