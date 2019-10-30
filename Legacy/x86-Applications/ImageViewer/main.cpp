#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <gfx/window/filedialog.h>
#include <gfx/window/messagebox.h>
#include <unistd.h>
#include <fcntl.h>

uint8_t* buffer;
void OnPaint(surface_t* surface){
	DrawBitmapImage(0,0,surface->width, surface->height, buffer, surface);
}

extern "C"
int main(int argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 100;
	windowInfo.height = 100;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	char* file = FileDialog("/");
	if(file){
		int fd = open(file, 0);
		int size = lseek(fd, 0, SEEK_END);
		buffer = (uint8_t*)malloc(size);
		memset(buffer, 0, size);
		read(fd, buffer, size);
		close(fd);
	} else {
		MessageBox("Failed to read image! Exiting.", MESSAGEBOX_OK);
		exit();
		for(;;);
	}
	bitmap_file_header_t bmpFileHeader = *((bitmap_file_header_t*)buffer);
	bitmap_info_header_t bmpInfoHeader = *((bitmap_info_header_t*)(buffer + sizeof(bitmap_file_header_t)));

	if(bmpFileHeader.magic[0] != 'B' || bmpFileHeader.magic[1] != 'M'){
		MessageBox("Invalid or corrupted BMP File! Exiting.", MESSAGEBOX_OK);
		exit();
		for(;;);
	}

	if(windowInfo.width > 60)
		windowInfo.width = bmpInfoHeader.width;
	if(windowInfo.height > 60)
		windowInfo.height = bmpInfoHeader.height;

	Window* win = CreateWindow(&windowInfo);
	win->OnPaint = OnPaint;

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
				return 0;
			}
		}

		PaintWindow(win);
	}
}