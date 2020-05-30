#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/window.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
#include <stdlib.h>
#include <list.h>
#include <lemon/filesystem.h>
#include <fcntl.h>
#include <unistd.h>
#include <gui/messagebox.h>
#include <lemon/spawn.h>
#include <lemon/util.h>

void OnFileOpened(char* path, char** filePointer){
	if(strncmp(path + strlen(path) - 4, ".lef", 4) == 0){
		lemon_spawn(path, 1, &path);
	} else if(strncmp(path + strlen(path) - 4, ".txt", 4) == 0 || strncmp(path + strlen(path) - 4, ".cfg", 4) == 0){
		char* argv[] = {"/initrd/textedit.lef", path};
		lemon_spawn("/initrd/textedit.lef", 2, argv);
	} else if(strncmp(path + strlen(path) - 4, ".png", 4) == 0 || strncmp(path + strlen(path) - 4, ".bmp", 4) == 0){
		char* argv[] = {"/initrd/imgview.lef", path};
		lemon_spawn("/initrd/imgview.lef", 2, argv);
	}
}

extern "C"
int main(int argc, char** argv){
	win_info_t windowInfo;
	Lemon::GUI::Window* window;

	windowInfo.width = 512;
	windowInfo.height = 256;
	windowInfo.x = 100;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "FileMan");

	window = Lemon::GUI::CreateWindow(&windowInfo);

	char* filePointer = nullptr;
	
	Lemon::GUI::FileView* fv = new Lemon::GUI::FileView({{0,0},{512,256}}, "/", &filePointer, OnFileOpened);

	AddWidget(fv, window);
	
	bool repaint = true;

	for(;;){
		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				Lemon::GUI::HandleMouseUp(window, {mouseX, mouseY});
				repaint = true;
			} else if (msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				Lemon::GUI::HandleMouseDown(window, {mouseX, mouseY});
				repaint = true;
			} else if (msg.msg == WINDOW_EVENT_KEY && msg.data == '\n') {
				fv->OnSubmit();
			} else if (msg.msg == WINDOW_EVENT_MOUSEMOVE) {
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				Lemon::GUI::HandleMouseMovement(window, {mouseX, mouseY});
				repaint = true;
			} else if (msg.msg == WINDOW_EVENT_KEY) {
				Lemon::GUI::HandleKeyPress(window, msg.data);

				repaint = true;
			} else if (msg.msg == WINDOW_EVENT_CLOSE) {
				DestroyWindow(window);
				exit(0);
			}
		}

		if(repaint)
			Lemon::GUI::PaintWindow(window);

		lemon_yield();
	}

	for(;;);
}