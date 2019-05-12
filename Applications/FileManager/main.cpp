#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>

typedef struct fs_dirent {
	uint32_t inode; // Inode number
	char name[128]; // Filename
} fs_dirent_t;

struct FileButton : public Button{
	bool executable;
	fs_dirent_t dirent;

	char filepath[128];

	FileButton(char* label, rect_t bounds) : Button::Button(label, bounds){
		//Button::Button(label, bounds);
	}

	void OnMouseUp(){
		Button::pressed = false;
		if(executable){
			syscall(SYS_EXEC, (uint32_t)filepath,0,0,0,0);
		}
	}
};

extern "C"
void pmain(){
	win_info_t windowInfo;

	windowInfo.width = 400;
	windowInfo.height = 300;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	Window* win = CreateWindow(&windowInfo);

	unsigned int fd;
	syscall(SYS_OPEN, (uint32_t)"/",(uint32_t)&fd, 0, 0, 0);

	fs_dirent_t dirent;
		uint32_t ret;
		syscall(SYS_READDIR, fd, (uint32_t)&dirent, 0,(uint32_t)&ret,0);
		int xpos = 10;
		int ypos = 10;
		int i = 0;
		while(ret){
			//DrawBitmapImage(0, 0, 48, 48, (uint8_t*)(fileIconBMP + 54), &windowSurface);
			//if(!readReturn) DrawRect(0,0,20,20,255,0,0, &windowSurface);

			FileButton* btn = new FileButton(dirent.name, {{xpos, ypos},{120,24}});
			btn->filepath[0] = '/';
			btn->filepath[1] = '\0';
			strcat(btn->filepath, dirent.name);
			if(btn->filepath[strlen(btn->filepath)-1] == 'f' && btn->filepath[strlen(btn->filepath)-2] == 'e' && btn->filepath[strlen(btn->filepath)-3] == 'l' && btn->filepath[strlen(btn->filepath)-4] == '.')
				btn->executable = true;
			win->AddWidget(btn);

			syscall(SYS_READDIR, fd, (uint32_t)&dirent, ++i,(uint32_t)&ret,0);

			xpos += 128;
			if(xpos + 128 > 400){
				ypos += 32;
				xpos = 10;
			}
		}

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
			}
		}

		PaintWindow(win);
	}
}

/*
extern "C"
void pmain(){
	win_info_t windowInfo;
	handle_t windowHandle;

	windowInfo.width = 400;
	windowInfo.height = 300;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	windowHandle = CreateWindow(&windowInfo);

	surface_t windowSurface;
	windowSurface.buffer = (uint8_t*)malloc(400*300*4);
	windowSurface.width = 400;
	windowSurface.height = 300;
	
	unsigned int fd;
	syscall(SYS_OPEN, (uint32_t)"/",(uint32_t)&fd, 0, 0, 0);

	void* fileIconBMP;
	void* folderIconBMP;
	void* binFileIconBMP;

	int fileIconFd;
	syscall(SYS_OPEN, (uint32_t)"/file.bmp",(uint32_t)&fileIconFd, 0, 0, 0);
	int folderIconFd;
	syscall(SYS_OPEN, (uint32_t)"/folder.bmp",(uint32_t)&folderIconFd, 0, 0, 0);
	int binFileIconFd;
	syscall(SYS_OPEN, (uint32_t)"/binaryfile.bmp",(uint32_t)&binFileIconFd, 0, 0, 0);

	int fileIconBMPSize;
	int folderIconBMPSize;
	int binFileIconBMPSize;

	/*syscall(SYS_LSEEK, fileIconFd, 0, 2, (uint32_t)&fileIconBMPSize, 0);
	syscall(SYS_LSEEK, binFileIconFd, 0, 2, (uint32_t)&binFileIconBMPSize, 0);
	syscall(SYS_LSEEK, folderIconFd, 0, 2, (uint32_t)&folderIconBMPSize, 0);

	fileIconBMP = malloc(fileIconBMPSize);
	folderIconBMP = malloc(folderIconBMPSize);
	binFileIconBMP = malloc(binFileIconBMPSize);

	uint32_t readReturn;
	syscall(SYS_READ, fileIconFd, (uint32_t)fileIconBMP,fileIconBMPSize,(uint32_t)&readReturn,0);
	syscall(SYS_READ, folderIconFd, (uint32_t)folderIconBMP,folderIconBMPSize,0,0);
	syscall(SYS_READ, binFileIconFd, (uint32_t)binFileIconBMP,binFileIconBMPSize,0,0);
	* /for(;;){
		DrawRect(0,0,400,300,220,220,220,&windowSurface);

		fs_dirent_t dirent;
		uint32_t ret;
		syscall(SYS_READDIR, fd, (uint32_t)&dirent, 0,(uint32_t)&ret,0);
		int xpos = 4;
		int ypos = 4;
		int i = 0;
		while(ret){
			//DrawBitmapImage(0, 0, 48, 48, (uint8_t*)(fileIconBMP + 54), &windowSurface);
			//if(!readReturn) DrawRect(0,0,20,20,255,0,0, &windowSurface);

			DrawString(dirent.name, xpos,ypos,0,0,0,&windowSurface);

			syscall(SYS_READDIR, fd, (uint32_t)&dirent, ++i,(uint32_t)&ret,0);

			/*xpos += 128;
			if(xpos + 128 > 400){
				ypos += 64;
				xpos = 10;
			}* /
			ypos += 24;
		}

		syscall(SYS_PAINT_WINDOW, (uint32_t)windowHandle, (uint32_t)&windowSurface,0,0,0);
	}

	for(;;);
}
*/