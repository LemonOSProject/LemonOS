#include <stdint.h>

#include <lemon/syscall.h>
#include <lemon/system/filesystem.h>
#include <lemon/system/spawn.h>
#include <lemon/system/info.h>
#include <lemon/system/util.h>
#include <lemon/system/fb.h>
#include <lemon/system/ipc.h>
#include <lemon/system/waitable.h>
#include <lemon/gui/window.h>
#include <lemon/core/sharedmem.h>
#include <lemon/core/shell.h>
#include <lemon/core/keyboard.h>
#include <lemon/gfx/graphics.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <map>

#include "shell.h"

#define MENU_ITEM_HEIGHT 24

fb_info_t videoInfo;
Lemon::GUI::Window* taskbar;
extern Lemon::GUI::Window* menuWindow;
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

bool paintTaskbar = true;
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
	Lemon::Graphics::DrawGradientVertical(0,0,surface->width, surface->height, {0x33, 0x2c, 0x29, 255}, {0x2e, 0x29, 0x29, 255},surface);

	if(showMenu){
		Lemon::Graphics::surfacecpyTransparent(surface, &menuButton, {18 - menuButton.width / 2, 18 - menuButton.height / 4}, {0, menuButton.height / 2, menuButton.width, 30});
	} else {
		Lemon::Graphics::surfacecpyTransparent(surface, &menuButton, {18 - menuButton.width / 2, 18 - menuButton.height / 4}, {0, 0, menuButton.width, 30});
	}

	sprintf(memString, "Used Memory: %lu/%lu KB", sysInfo.usedMem, sysInfo.totalMem);
	Lemon::Graphics::DrawString(memString, surface->width - Lemon::Graphics::GetTextLength(memString) - 8, 10, 255, 255, 255, surface);
}

void InitializeMenu();
void PollMenu();
void MinimizeMenu(bool s);

int main(){
	handle_t svc = Lemon::CreateService("lemon.shell");
	shell = new ShellInstance(svc, "Instance");

	syscall(SYS_GET_VIDEO_MODE, (uintptr_t)&videoInfo,0,0,0,0);
	syscall(SYS_UNAME, (uintptr_t)versionString,0,0,0,0);

	Lemon::Graphics::LoadImage("/initrd/menubuttons.png", &menuButton);

	handle_t tempEndpoint = 0;
	while(tempEndpoint <= 0){
		tempEndpoint = Lemon::InterfaceConnect("lemon.lemonwm/Instance");
	} // Wait for LemonWM to create the interface (if not already created)
	Lemon::DestroyKObject(tempEndpoint);

	taskbar = new Lemon::GUI::Window("", {static_cast<int>(videoInfo.width), 36}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::GUI, {0, static_cast<int>(videoInfo.height) - 36});
	taskbar->OnPaint = OnTaskbarPaint;
	taskbar->rootContainer.background = {0, 0, 0, 0};
	taskbarWindowsContainer = new Lemon::GUI::LayoutContainer({40, 0, static_cast<int>(videoInfo.width) - 104, static_cast<int>(videoInfo.height)}, {160, 36 - 4});
	taskbarWindowsContainer->background = {0, 0, 0, 0};
	taskbar->AddWidget(taskbarWindowsContainer);
	
	shell->AddWindow = AddWindow;
	shell->RemoveWindow = RemoveWindow;

	InitializeMenu();

	taskbar->InitializeShellConnection();

	Lemon::Waiter waiter;
	waiter.WaitOnAll(&shell->GetInterface());
	waiter.WaitOn(taskbar);
	waiter.WaitOn(menuWindow);

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
		sysInfo = Lemon::SysInfo();

		if(sysInfo.usedMem != usedMemLast) paintTaskbar = true;

		if(paintTaskbar){
			taskbar->Paint();

			paintTaskbar = false;
		}

		waiter.Wait();
	}

	for(;;);
}
