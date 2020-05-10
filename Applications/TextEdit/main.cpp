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

#include "exttextbox.h"

ExtendedTextBox* textBox;
Lemon::GUI::Window* window;

void OnWindowPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, window->info.height - 20, window->info.width, 20, 160, 160, 160, surface);

	char buf[32];
	sprintf(buf, "Line: %d    Col: %d", textBox->cursorPos.y + 1, textBox->cursorPos.x + 1); // lines and columns don't start at 0

	Lemon::Graphics::DrawString(buf, 4, window->info.height - 20 + 4, 0, 0, 0, surface);
}

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 512;
	windowInfo.height = 256;
	windowInfo.x = 100;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "TextEdit");

	char* filePath;

	if(argc > 1){
		filePath = argv[1];
	} else {
		filePath = Lemon::GUI::FileDialog("/");

		if(!filePath){
			Lemon::GUI::MessageBox("Invalid filepath!", MESSAGEBOX_OK);
			exit(1);
		}
	}

	window = Lemon::GUI::CreateWindow(&windowInfo);
	window->OnPaint = OnWindowPaint;

	textBox = new ExtendedTextBox({{0, 0}, {windowInfo.width, windowInfo.height - 20}});

	window->widgets.add_back(textBox);

	FILE* textFile = fopen(filePath, "r");

	if(!textFile){
		Lemon::GUI::MessageBox("Failed to open file!", MESSAGEBOX_OK);
		exit(1);
	}

	fseek(textFile, 0, SEEK_END);
	size_t textFileSize = ftell(textFile);
	fseek(textFile, 0, SEEK_SET);

	char* textBuffer = (char*)malloc(textFileSize + 1);

	fread(textBuffer, textFileSize, 1, textFile);
	for(int i = 0; i < textFileSize; i++){ if(textBuffer[i] == 0) textBuffer[i] = ' ';}
	textBuffer[textFileSize] = 0;

	textBox->font = Lemon::Graphics::LoadFont("/initrd/sourcecodepro.ttf", "monospace", 12);
	textBox->multiline = true;
	textBox->editable = true;
	textBox->LoadText(textBuffer);

	free(textBuffer);

	for(;;){
		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = (msg.data >> 32);
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				Lemon::GUI::HandleMouseDown(window, {(int)mouseX, (int)mouseY});
			}
			else if(msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				Lemon::GUI::HandleMouseUp(window, {(int)mouseX, (int)mouseY});
			} else if (msg.msg == WINDOW_EVENT_MOUSEMOVE) {
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				Lemon::GUI::HandleMouseMovement(window, {mouseX, mouseY});
			}  else if (msg.msg == WINDOW_EVENT_KEY) {
				Lemon::GUI::HandleKeyPress(window, msg.data);
			} else if (msg.msg == WINDOW_EVENT_CLOSE) {
				Lemon::GUI::DestroyWindow(window);
				exit(0);
			}
		}

		Lemon::GUI::PaintWindow(window);
	}

	for(;;);
}
