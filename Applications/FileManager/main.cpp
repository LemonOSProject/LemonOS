#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <core/filesystem.h>

uint8_t* fileIconBuffer;
uint8_t* exIconBuffer;
uint8_t* folderIconBuffer;

unsigned int fd;

Window* win;
win_info_t windowInfo;

void RefreshFiles();

struct FileButton : public Button{
	bool executable;
	lemon_dirent_t dirent;

	int icon = 0;

	char filepath[128];

	FileButton(char* label, rect_t bounds) : Button::Button(label, bounds){
		//Button::Button(label, bounds);
	}

	void OnMouseUp(){
		Button::pressed = false;
		if(executable){
			syscall(SYS_EXEC, (uint32_t)filepath,0,0,0,0);
		}
		
		if (icon == 2){
			lemon_close(fd);
			fd = 0;
			fd = lemon_open(filepath, 0);
			if(fd) {
				win->widgets.clear();
				//RefreshFiles();
			}
		}
	}

	void Paint(surface_t* surface){
		if(pressed){
			DrawRect(bounds.pos.x+1, bounds.pos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
			DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);
			
			DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
			DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
			DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
			DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
		}

		if(icon == 0)
			DrawBitmapImage(bounds.pos.x + bounds.size.x / 2 - 24,bounds.pos.y,48,48,fileIconBuffer,surface);
		else if (icon == 1)
			DrawBitmapImage(bounds.pos.x + bounds.size.x / 2 - 24,bounds.pos.y,48,48,exIconBuffer,surface);
		else if (icon == 2)
			DrawBitmapImage(bounds.pos.x + bounds.size.x / 2 - 24,bounds.pos.y,48,48,folderIconBuffer,surface);

		DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y - 16, 0, 0, 0, surface);
	}
};

void RefreshFiles(){
	lemon_dirent_t dirent;
	uint32_t ret;
	//syscall(SYS_READDIR, fd, (uint32_t)&dirent, 0,(uint32_t)&ret,0);
	ret = lemon_readdir(fd, 0, &dirent);
	int xpos = 0;
	int ypos = 0;
	int i = 0;
	while(ret){
		FileButton* btn = new FileButton(dirent.name, {{xpos + 2, ypos + 2},{96,72}});
		btn->filepath[0] = '/';
		btn->filepath[1] = '\0';
		strcat(btn->filepath, dirent.name);
		if(btn->filepath[strlen(btn->filepath)-1] == 'f' && btn->filepath[strlen(btn->filepath)-2] == 'e' && btn->filepath[strlen(btn->filepath)-3] == 'l' && btn->filepath[strlen(btn->filepath)-4] == '.'){
			btn->executable = true;
			btn->icon = 1;
		}

		if(dirent.type == FS_NODE_DIRECTORY){
			btn->icon = 2;
		}
		AddWidget(btn, win);

		ret = lemon_readdir(fd, ++i, &dirent);

		xpos += 100;
		if(xpos + 100 > windowInfo.width){
			ypos += 76;
			xpos = 0;
		}
	}

}

extern "C"
int main(char argc, char** argv){

	windowInfo.width = 500;
	windowInfo.height = 500;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "File Manager");

	win = CreateWindow(&windowInfo);

	fd = lemon_open("/", 0);

	FILE* icon = fopen("/file.bmp","r");

	size_t fileIconBufferSize = ftell(icon);
	fileIconBuffer = (uint8_t*)malloc(fileIconBufferSize);
	fread(fileIconBuffer, fileIconBufferSize, 1, icon);

	fclose(icon);

	icon = fopen("/binfile.bmp","r");
	
	fileIconBufferSize = ftell(icon);
	exIconBuffer = (uint8_t*)malloc(fileIconBufferSize);
	fread(exIconBuffer, fileIconBufferSize, 1, icon);

	fclose(icon);

	icon = fopen("/folder.bmp","r");
	
	fileIconBufferSize = ftell(icon);
	folderIconBuffer = (uint8_t*)malloc(fileIconBufferSize);
	fread(folderIconBuffer, fileIconBufferSize, 1, icon);

	fclose(icon);

	RefreshFiles();

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch(msg.data){
				default:
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
				exit();
				for(;;);
			}
		}
		PaintWindow(win);
		asm("hlt");
	}
}