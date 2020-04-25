#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
#include <stdlib.h>
#include <list.h>
#include <lemon/filesystem.h>
#include <fcntl.h>
#include <unistd.h>
#include <gfx/window/messagebox.h>
#include <lemon/spawn.h>

void OnFileOpened(char* path, char** filePointer){
	
}

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;
	Window* window;

	windowInfo.width = 512;
	windowInfo.height = 256;
	windowInfo.x = 100;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "FileMan");

	window = CreateWindow(&windowInfo);

	char* filePointer = nullptr;
	FileView* fv = new FileView({{0,0},{512,256}}, "/", &filePointer, OnFileOpened);

	AddWidget(fv, window);

	PaintWindow(window);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseUp(window, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseDown(window, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_KEY && msg.data == '\n') {
				fv->OnSubmit();
			} else if (msg.msg == WINDOW_EVENT_MOUSEMOVE) {
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseMovement(window, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_CLOSE) {
				DestroyWindow(window);
				exit(0);
			}
		}
		PaintWindow(window);
	}

	for(;;);
}