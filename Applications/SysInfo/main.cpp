#include <lemon/gui/window.h>
#include <lemon/gui/widgets.h>
#include <lemon/gui/messagebox.h>
#include <lemon/gui/filedialog.h>
#include <lemon/system/info.h>
#include <lemon/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

Lemon::GUI::Window* window;
surface_t banner;
Lemon::GUI::Bitmap* bannerW;

Lemon::GUI::Label* totalMem;
Lemon::GUI::Label* usedMem;

char versionString[80];

lemon_sysinfo_t sysInfo;

int main(int argc, char** argv){
    Lemon::Graphics::LoadImage("/initrd/banner.png", &banner);
    
    bannerW = new Lemon::GUI::Bitmap({{0, 0}, {300, banner.height}}, &banner);

    int winWidth = (banner.width > 200) ? banner.width : 200; 

    window = new Lemon::GUI::Window("System Information", {winWidth, 300}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
    window->AddWidget(bannerW);

    int ypos = banner.height + 4;
    sysInfo = Lemon::SysInfo();

    char buf[64];

	syscall(SYS_UNAME, versionString,0,0,0,0);

    window->AddWidget(new Lemon::GUI::Label(versionString, {{4, ypos}, {200, 12}}));
    ypos += 24;

    snprintf(buf, 64, "Total System Memory: %lu MB (%lu KB)", sysInfo.totalMem / 1024, sysInfo.totalMem);
    totalMem = new Lemon::GUI::Label(buf, {{4, ypos}, {200, 12}});
    window->AddWidget(totalMem);
    ypos += 16;
    
    snprintf(buf, 64, "Used System Memory: %lu MB (%lu KB)", sysInfo.usedMem / 1024, sysInfo.usedMem);
    usedMem = new Lemon::GUI::Label(buf, {{4, ypos}, {200, 12}});
    window->AddWidget(usedMem);
    ypos += 16;

	while(!window->closed){
		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			window->GUIHandleEvent(ev);
		}

		window->Paint();

        lemon_sysinfo_t _sysInfo = Lemon::SysInfo();

        if(_sysInfo.usedMem != sysInfo.usedMem){
            snprintf(buf, 64, "Used System Memory: %lu MB (%lu KB)", sysInfo.usedMem / 1024, sysInfo.usedMem);
            usedMem->label = buf;
        } sysInfo = _sysInfo;

        window->WaitEvent();
	}
}