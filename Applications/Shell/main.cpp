#include <stdint.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <gfx/graphics.h>
#include <lemon/filesystem.h>
#include <gui/window.h>
#include <stdio.h>
#include <list.h>
#include <core/keyboard.h>
#include <fcntl.h>
#include <unistd.h>
#include <lemon/spawn.h>
#include <lemon/info.h>
#include <core/sharedmem.h>
#include <lemon/util.h>
#include <core/shell.h>
#include <map>

#include "shell.h"

#define MENU_ITEM_HEIGHT 24

fb_info_t videoInfo;
Lemon::GUI::Window* taskbar;
Lemon::GUI::Window* menu;
ShellInstance* shell;

bool showMenu = true;

char versionString[80];

struct MenuItem{
	char name[64];
	char path[64];
};

MenuItem menuItems[32];
int menuItemCount = 0;
lemon_sysinfo_t sysInfo;
char memString[128];

class WindowButton : public Lemon::GUI::Button {
	ShellWindow* win;

public:
	WindowButton(ShellWindow* win, rect_t bounds) : Button(win->title.c_str(), bounds){
		this->win = win;
		labelAlignment = Lemon::GUI::TextAlignment::Left;
	}

	void Paint(surface_t* surface){
		if(win->state == Lemon::Shell::ShellWindowStateActive || pressed){
			Lemon::Graphics::DrawRect(fixedBounds, {96, 96, 96}, surface);
		}

		DrawButtonBorders(surface, false);
		DrawButtonLabel(surface, true);
	}

	void OnMouseUp(vector2i_t mousePos){
		pressed = false;

		if(win->lastState == Lemon::Shell::ShellWindowStateActive){
			window->Minimize(win->id, true);
		} else {
			window->Minimize(win->id, false);
		}
	}
};

std::map<ShellWindow*, WindowButton*> taskbarWindows;
Lemon::GUI::LayoutContainer* taskbarWindowsContainer;

void AddWindow(ShellWindow* win){
	WindowButton* btn = new WindowButton(win, {0, 0, 0, 0} /* The LayoutContainer will handle boudns for us*/);
	taskbarWindows.insert(std::pair<ShellWindow*, WindowButton*>(win, btn));

	taskbarWindowsContainer->AddWidget(btn);
}

void RemoveWindow(ShellWindow* win){
	WindowButton* btn = taskbarWindows[win];
	taskbarWindows.erase(win);

	taskbarWindowsContainer->RemoveWidget(btn);
	delete btn;
}

void OnTaskbarPaint(surface_t* surface){
	Lemon::Graphics::DrawGradientVertical(100,0,surface->width - 100, /*surface->height*/24, {96, 96, 96}, {42, 50, 64},surface);
	Lemon::Graphics::DrawRect(100,24,surface->width - 100, surface->height - 24, {42,50,64},surface);

	if(showMenu){
		Lemon::Graphics::DrawGradientVertical(0,0,100, 24, {120,12,12}, {/*160*/60, /*16*/6, /*16*/6},surface);
		Lemon::Graphics::DrawRect(0,24,100, surface->height - 24, {/*160*/60, /*16*/6, /*16*/6},surface);
	} else {
		Lemon::Graphics::DrawGradientVertical(0,0,100, 24, {220,/*24*/48,30}, {/*160*/120, /*16*/12, /*16*/12},surface);
		Lemon::Graphics::DrawRect(0,24,100, surface->height - 24, {/*160*/120, /*16*/12, /*16*/12},surface);
	}

	sprintf(memString, "Used Memory: %d/%d KB", sysInfo.usedMem, sysInfo.totalMem);
	Lemon::Graphics::DrawString(memString, surface->width - Lemon::Graphics::GetTextLength(memString) - 8, 10, 255, 255, 255, surface);
}

void OnMenuPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0,32,surface->width, surface->height - 32, {64,64,64}, surface);
	Lemon::Graphics::DrawGradientVertical(0,0,surface->width, 32, {220,/*24*/48,30}, {/*160*/120, /*16*/12, /*16*/12},surface);
	Lemon::Graphics::DrawString(versionString,5,MENU_ITEM_HEIGHT / 2 - 6,255,255,255,surface);

	for(int i = 0; i < menuItemCount; i++){
		Lemon::Graphics::DrawString(menuItems[i].name, 5, 42 + i * MENU_ITEM_HEIGHT /* 2 pixels padding */, 255, 255, 255, surface);
	}
}

void LoadConfig(){
	FILE* menuConfig = fopen("/initrd/menu.cfg","r");

	if(!menuConfig){
		return;
	}

	fseek(menuConfig, 0, SEEK_END);
	size_t configSize = ftell(menuConfig);

	fseek(menuConfig, 0, SEEK_SET);

	char* config = (char*)malloc(configSize);

	fread(config, configSize, 1, menuConfig);

	char name[64];
	char buffer[64];
	int namePos = 0;
	int bufPos = 0;
	memset(buffer,0,64);
	memset(name,0,64);
	for(int i = 0; i < configSize; i++){
		switch(config[i]){
		case '\r':
			break;
		case '\n':
			if(!namePos){
				namePos = 0;
				bufPos = 0;
				break;
			}
			menuItemCount++;
			
			strcpy(menuItems[menuItemCount-1].name, name);
			strcpy(menuItems[menuItemCount-1].path, buffer);
			
			memset(buffer,0,64);
			memset(name,0,64);
			namePos = 0;
			bufPos = 0;
			break;
		case ':':
			strncpy(name, buffer, bufPos);
			namePos = bufPos;
			bufPos = 0;
			memset(buffer,0,64);
			break;
		default:
			buffer[bufPos++] = config[i];
			break;
		}
	}
}

bool paint = true;

int main(){
	sockaddr_un shellAddress;
	strcpy(shellAddress.sun_path, Lemon::Shell::shellSocketAddress);
	shellAddress.sun_family = AF_UNIX;
	shell = new ShellInstance(shellAddress);

	syscall(SYS_GET_VIDEO_MODE, (uintptr_t)&videoInfo,0,0,0,0);
	syscall(SYS_UNAME, (uintptr_t)versionString,0,0,0,0);

	taskbar = new Lemon::GUI::Window("", {videoInfo.width, 30}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::GUI, {0, videoInfo.height - 30});
	taskbar->OnPaint = OnTaskbarPaint;
	taskbar->rootContainer.background = {0, 0, 0, 0};
	taskbarWindowsContainer = new Lemon::GUI::LayoutContainer({100, 0, videoInfo.width - 104, videoInfo.height}, {128, 30 - 4});
	taskbarWindowsContainer->background = {0, 0, 0, 0};
	taskbar->AddWidget(taskbarWindowsContainer);

	menu = new Lemon::GUI::Window("", {240, 300}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::Basic, {0, videoInfo.height - 30 - 300});
	menu->OnPaint = OnMenuPaint;

	Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::GUI::WMCommand));
	Lemon::GUI::WMCommand* cmd = (Lemon::GUI::WMCommand*)msg->data;
	cmd->cmd = Lemon::GUI::WMInitializeShellConnection;
	msg->protocol = LEMON_MESSAGE_PROTCOL_WMCMD;
	msg->length = sizeof(Lemon::GUI::WMCommand);
	taskbar->SendWMMsg(msg);
	free(msg);

	LoadConfig();
	
	shell->AddWindow = AddWindow;

	for(;;){
		shell->Update();

		Lemon::LemonEvent ev;
		while(taskbar->PollEvent(ev)){
			if(ev.event == Lemon::EventMouseReleased){
				if(ev.mousePos.x < 100){
					showMenu = !showMenu;

					menu->Minimize(!showMenu);

					paint = true;
				} else {
					taskbar->GUIHandleEvent(ev);
				}
			} else {
				taskbar->GUIHandleEvent(ev);
			}
		}

		while(menu->PollEvent(ev)){
			if(ev.event == Lemon::EventMouseReleased){
				if(ev.mousePos.y > 42 && ev.mousePos.y < (menuItemCount*MENU_ITEM_HEIGHT + 42)){
					char* const argv[] = {menuItems[(int)floor((double)(ev.mousePos.y - 42) / MENU_ITEM_HEIGHT)].path};
					lemon_spawn(argv[0], 1, argv, 0);

					showMenu = false;
					menu->Minimize(!showMenu);

					paint = true;
				}
			}
		}

		uint64_t usedMemLast = sysInfo.usedMem;
		syscall(SYS_INFO, &sysInfo, 0, 0, 0, 0);

		if(sysInfo.usedMem != usedMemLast) paint = true;

		if(paint){

			if(showMenu){
				menu->Paint();
			}

			paint = false;
		}
		taskbar->Paint();

		lemon_yield();
	}

	for(;;);
}
