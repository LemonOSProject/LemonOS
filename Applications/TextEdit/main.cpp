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
#include <unistd.h>
#include <sys/stat.h>

#include "exttextbox.h"

#define TEXTEDIT_OPEN 1
#define TEXTEDIT_SAVEAS 2
#define TEXTEDIT_SAVE 3

ExtendedTextBox* textBox;
Lemon::GUI::Window* window;
Lemon::GUI::WindowMenu fileMenu;
std::string openPath;

void LoadFile(const char* path){
	textBox->contents.clear();

	FILE* textFile = fopen(path, "r");

	if(!textFile){
		Lemon::GUI::DisplayMessageBox("Text Editor", "Failed to open file!", Lemon::GUI::MsgButtonsOK);
		return;
	}

	fseek(textFile, 0, SEEK_END);
	size_t textFileSize = ftell(textFile);
	fseek(textFile, 0, SEEK_SET);

	char* textBuffer = (char*)malloc(textFileSize + 1);

	fread(textBuffer, textFileSize, 1, textFile);
	for(int i = 0; i < textFileSize; i++){ if(textBuffer[i] == 0) textBuffer[i] = ' ';}
	textBuffer[textFileSize] = 0;
	
	textBox->LoadText(textBuffer);
	
	free(textBuffer);

	openPath = path;

	char title[256];
	sprintf(title, "Text Editor: %s", openPath.c_str());

	window->SetTitle(title);
}

void SaveFile(const char* path){
	struct stat sResult;
	int ret = stat(path, &sResult);
	if(S_ISDIR(sResult.st_mode)){
		Lemon::GUI::DisplayMessageBox("Text Editor", "File is a directory!", Lemon::GUI::MsgButtonsOK);
		return;
	}

	FILE* textFile = fopen(path, "w");

	if(!textFile){
		Lemon::GUI::DisplayMessageBox("Text Editor", "Failed to open file for writing!", Lemon::GUI::MsgButtonsOK);
		return;
	}

	fseek(textFile, 0, SEEK_SET);

	for(std::string& str : textBox->contents){
		fwrite(str.c_str(), 1, str.length(), textFile);
		fwrite("\n", 1, 1, textFile); // Line ending
	}

	fclose(textFile);

	openPath = path;
}

void OpenFile(){

	char* filePath = Lemon::GUI::FileDialog("/");

	if(!filePath){
		Lemon::GUI::DisplayMessageBox("Message", "Invalid filepath!", Lemon::GUI::MsgButtonsOK);
		return;
	}

	LoadFile(filePath);
}

void SaveFileAs(){
	char* filePath = Lemon::GUI::FileDialog(".", FILE_DIALOG_CREATE);

	if(!filePath){
		Lemon::GUI::DisplayMessageBox("Message", "Invalid filepath!", Lemon::GUI::MsgButtonsOK);
		return;
	}

	SaveFile(filePath);
}

void SaveOpenFile(){
	if(!openPath.length()){
		SaveFileAs();
	} else {
		SaveFile(openPath.c_str());
	}
}

void OnWindowPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, window->GetSize().y - 20, window->GetSize().x, 20, 160, 160, 160, surface);

	char buf[32];
	sprintf(buf, "Line: %d    Col: %d", textBox->cursorPos.y + 1, textBox->cursorPos.x + 1); // lines and columns don't start at 0

	Lemon::Graphics::DrawString(buf, 4 + WINDOW_MENUBAR_HEIGHT, window->GetSize().y - 20 + 4, 0, 0, 0, surface);
}

void OnWindowCmd(unsigned short cmd, Lemon::GUI::Window* win){
	if(cmd == TEXTEDIT_OPEN){
		OpenFile();
	} else if(cmd == TEXTEDIT_SAVEAS){
		SaveFileAs();
	} else if(cmd == TEXTEDIT_SAVE){
		SaveOpenFile();
	}
}

int main(int argc, char** argv){
	char* filePath;

	window = new Lemon::GUI::Window("Text Editor", {512, 256}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
	window->CreateMenuBar();

	fileMenu.first = "File";
	fileMenu.second.push_back({.id = TEXTEDIT_OPEN, .name = std::string("Open...")});
	fileMenu.second.push_back({.id = TEXTEDIT_SAVEAS, .name = std::string("Save As...")});
	fileMenu.second.push_back({.id = TEXTEDIT_SAVE, .name = std::string("Save...")});

	window->menuBar->items.push_back(fileMenu);

	window->OnMenuCmd = OnWindowCmd;
	window->OnPaint = OnWindowPaint;

	textBox = new ExtendedTextBox({{0, 0}, {0, 20}});
	window->AddWidget(textBox);
	textBox->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);

	textBox->font = Lemon::Graphics::LoadFont("/initrd/sourcecodepro.ttf", "monospace", 12);
	textBox->multiline = true;
	textBox->editable = true;

	if(argc > 1){
		LoadFile(argv[1]);
	}

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
