#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/ipc.h>
#include <lemon/filesystem.h>
#include <fcntl.h>

struct FileButton : public Button{
public:
	bool executable;
	lemon_dirent_t dirent;

	int icon = 0;

	char filepath[128];

	bool* ret;
	char* returnPath;

	FileButton(char* label, rect_t bounds) : Button::Button(label, bounds){

	}

	void OnMouseUp(vector2i_t mousePos){
		*ret = true;
		memcpy(returnPath, filepath, 128);
	}
};

char* FileDialog(char* directoryPath){
	win_info_t dialogWindowInfo;
	dialogWindowInfo.x = 10;
	dialogWindowInfo.y = 10;
	dialogWindowInfo.width = 300;
	dialogWindowInfo.height = 284;
	dialogWindowInfo.flags = 0;
	strcpy(dialogWindowInfo.title, "Open...");
	Window* dialogWindow = CreateWindow(&dialogWindowInfo);

	char* returnPath = (char*)malloc(128);
	bool ret = false;

	lemon_dirent_t dirent;
	uint32_t readdirReturn;
	int fd = open(directoryPath,0);
	if(!fd) return 0;
	readdirReturn = lemon_readdir(fd, 0, &dirent);
	int xpos = 0;
	int ypos = 0;
	int i = 0;
	while(readdirReturn){
		FileButton* btn = new FileButton(dirent.name, {{xpos + 2, ypos + 2},{96,24}});
		strcpy(btn->filepath, directoryPath);
		btn->ret = &ret;
		btn->returnPath = returnPath;
		strcat(btn->filepath, dirent.name);

		AddWidget(btn, dialogWindow);

		readdirReturn = lemon_readdir(fd, ++i, &dirent);

		xpos += 100;
		if(xpos + 100 > dialogWindowInfo.width){
			ypos += 26;
			xpos = 0;
		}
	}
	lemon_close(fd);

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
				PaintWindow(dialogWindow);
			} else if (msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseDown(dialogWindow, {mouseX, mouseY});
				PaintWindow(dialogWindow);
			}
		}

		if(ret){
			break;
		}
	}

	RefreshFonts();

	DestroyWindow(dialogWindow);
	return returnPath;
}