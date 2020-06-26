#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/window.h>
#include <gui/widgets.h>
#include <stdlib.h>
#include <list.h>
#include <gui/filedialog.h>
#include <gui/messagebox.h>
#include <stdio.h>

#include "exttextbox.h"

ExtendedTextBox* textBox;
Lemon::GUI::Window* window;

void OnWindowPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, window->GetSize().y - 20, window->GetSize().x, 20, 160, 160, 160, surface);

	char buf[32];
	sprintf(buf, "Line: %d    Col: %d", textBox->cursorPos.y + 1, textBox->cursorPos.x + 1); // lines and columns don't start at 0

	Lemon::Graphics::DrawString(buf, 4, window->GetSize().y - 20 + 4, 0, 0, 0, surface);
}

int main(int argc, char** argv){

	char* filePath;

	if(argc > 1){
		filePath = argv[1];
	} else {
		filePath = Lemon::GUI::FileDialog("/");

		if(!filePath){
			Lemon::GUI::DisplayMessageBox("Message", "Invalid filepath!", Lemon::GUI::MsgButtonsOK);
			exit(1);
		}
	}

	window = new Lemon::GUI::Window("TextEdit", {512, 256}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);

	window->OnPaint = OnWindowPaint;

	textBox = new ExtendedTextBox({{0, 0}, {0, 20}});
	window->AddWidget(textBox);
	textBox->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);

	FILE* textFile = fopen(filePath, "r");

	if(!textFile){
		//Lemon::GUI::MessageBox("Failed to open file!", MESSAGEBOX_OK);
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

	while(!window->closed){
		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			window->GUIHandleEvent(ev);
		}

		window->Paint();
	}

	delete window;
	return 0;
}
