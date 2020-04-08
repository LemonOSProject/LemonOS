#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <gfx/window/widgets.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
#include <stdlib.h>
#include <list.h>
#include <gfx/window/filedialog.h>
#include <gfx/window/messagebox.h>
#include <stdio.h>

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;
	Window* window;

	windowInfo.width = 512;
	windowInfo.height = 256;
	windowInfo.x = 100;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "TextEdit");

	char* filePath = FileDialog("/");

	window = CreateWindow(&windowInfo);

	TextBox* textBox = new TextBox({{0, 0}, {512, 256}});

	window->widgets.add_back(textBox);

	FILE* textFile = fopen(filePath, "r");

	if(!textFile){
		syscall(0, (uintptr_t)"error", 0x69, 0, 0, 0);
		MessageBox("Failed to open file!", MESSAGEBOX_OK);
		exit(1);
	}

	fseek(textFile, 0, SEEK_END);
	size_t textFileSize = ftell(textFile);
	fseek(textFile, 0, SEEK_SET);

	char* textBuffer = (char*)malloc(textFileSize);

	fread(textBuffer, textFileSize, 1, textFile);

	textBox->multiline = true;
	textBox->editable = true;
	textBox->LoadText(textBuffer);

	free(textBuffer);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_CLOSE){
				DestroyWindow(window);
				exit(0);
			} else if(msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = (msg.data >> 32);
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseDown(window, {(int)mouseX, (int)mouseY});
			}
			else if(msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseUp(window, {(int)mouseX, (int)mouseY});
			}
		}

		PaintWindow(window);
	}

	for(;;);
}
