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
ShellInstance* shell;
surface_t menuButton;

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
			Lemon::Graphics::DrawRect(fixedBounds, {42, 50, 64, 255}, surface);
		}

		DrawButtonLabel(surface, false);
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
	WindowButton* btn = new WindowButton(win, {0, 0, 0, 0} /* The LayoutContainer will handle bounds for us*/);
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
	Lemon::Graphics::DrawGradientVertical(0,0,surface->width, surface->height, {0x29, 0x2c, 0x33, 255}, {0x29, 0x29, 0x2e, 255},surface);

	if(showMenu){
		Lemon::Graphics::surfacecpyTransparent(surface, &menuButton, {0, 1}, {0, menuButton.height / 2, menuButton.width, 30});
	} else {
		Lemon::Graphics::surfacecpyTransparent(surface, &menuButton, {0, 1}, {0, 0, menuButton.width, 30});
	}

	sprintf(memString, "Used Memory: %lu/%lu KB", sysInfo.usedMem, sysInfo.totalMem);
	Lemon::Graphics::DrawString(memString, surface->width - Lemon::Graphics::GetTextLength(memString) - 8, 10, 255, 255, 255, surface);
}

bool paintTaskbar = true;
void InitializeMenu();
void PollMenu();
Lemon::MessageHandler& GetMenuWindowHandler();
void MinimizeMenu(bool s);

int main(){
	sockaddr_un shellAddress;
	strcpy(shellAddress.sun_path, Lemon::Shell::shellSocketAddress);
	shellAddress.sun_family = AF_UNIX;
	shell = new ShellInstance(shellAddress);

	syscall(SYS_GET_VIDEO_MODE, (uintptr_t)&videoInfo,0,0,0,0);
	syscall(SYS_UNAME, (uintptr_t)versionString,0,0,0,0);

	Lemon::Graphics::LoadImage("/initrd/menubuttons.png", &menuButton);

	taskbar = new Lemon::GUI::Window("", {static_cast<int>(videoInfo.width), 32}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::GUI, {0, static_cast<int>(videoInfo.height) - 32});
	taskbar->OnPaint = OnTaskbarPaint;
	taskbar->rootContainer.background = {0, 0, 0, 0};
	taskbarWindowsContainer = new Lemon::GUI::LayoutContainer({40, 0, static_cast<int>(videoInfo.width) - 104, static_cast<int>(videoInfo.height)}, {160, 32 - 4});
	taskbarWindowsContainer->background = {0, 0, 0, 0};
	taskbar->AddWidget(taskbarWindowsContainer);
	
	shell->AddWindow = AddWindow;
	shell->RemoveWindow = RemoveWindow;

	InitializeMenu();

	Lemon::MessageMultiplexer mp;
	mp.AddSource(taskbar->GetHandler());
	mp.AddSource(GetMenuWindowHandler());
	mp.AddSource(shell->GetServer());

	Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::GUI::WMCommand));
	Lemon::GUI::WMCommand* cmd = (Lemon::GUI::WMCommand*)msg->data;
	cmd->cmd = Lemon::GUI::WMInitializeShellConnection;
	msg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;
	msg->length = sizeof(Lemon::GUI::WMCommand);
	taskbar->SendWMMsg(msg);
	free(msg);

	for(;;){
		shell->Update();

		Lemon::LemonEvent ev;
		while(taskbar->PollEvent(ev)){
			if(ev.event == Lemon::EventMouseReleased){
				if(ev.mousePos.x < 100){
					showMenu = !showMenu;

					MinimizeMenu(!showMenu);
				} else {
					taskbar->GUIHandleEvent(ev);
				}
			} else {
				taskbar->GUIHandleEvent(ev);
			}
			paintTaskbar = true;
		}

		PollMenu();

		uint64_t usedMemLast = sysInfo.usedMem;
		syscall(SYS_INFO, &sysInfo, 0, 0, 0, 0);

		if(sysInfo.usedMem != usedMemLast) paintTaskbar = true;

		if(paintTaskbar){
			taskbar->Paint();

			paintTaskbar = false;
		}

		mp.PollSync();
	}

	for(;;);
}
