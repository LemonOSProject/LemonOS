#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/ipc.h>
#include <lemon/filesystem.h>
#include <fcntl.h>

void OnFileOpened(char* path, char** filePointer){
	*filePointer = (char*)malloc(strlen(path) + 1);
	strcpy(*filePointer, path);
}

char* FileDialog(char* directoryPath){
	win_info_t dialogWindowInfo;
	dialogWindowInfo.x = 10;
	dialogWindowInfo.y = 10;
	dialogWindowInfo.width = 300;
	dialogWindowInfo.height = 280;
	dialogWindowInfo.flags = 0;
	strcpy(dialogWindowInfo.title, "Open...");
	Window* dialogWindow = CreateWindow(&dialogWindowInfo);

	char* filePointer = nullptr;
	FileView* fv = new FileView({{0,0},{300,280}}, directoryPath, &filePointer, OnFileOpened);

	AddWidget(fv, dialogWindow);

	PaintWindow(dialogWindow);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseUp(dialogWindow, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseDown(dialogWindow, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_KEY && msg.data == '\n') {
				fv->OnSubmit();

				if(filePointer) goto ret;
			} else if (msg.msg == WINDOW_EVENT_MOUSEMOVE) {
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseMovement(dialogWindow, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_CLOSE) {
				DestroyWindow(dialogWindow);
				return nullptr;
			}
		}
		PaintWindow(dialogWindow);
	}
ret:
	DestroyWindow(dialogWindow);

	return filePointer;
}