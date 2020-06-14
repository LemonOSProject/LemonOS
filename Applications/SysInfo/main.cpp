#include <gui/window.h>
#include <gui/widgets.h>
#include <gui/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <lemon/syscall.h>
#include <gui/filedialog.h>
#include <lemon/info.h>
#include <unistd.h>

Lemon::GUI::Window* window;
surface_t banner;
Lemon::GUI::Bitmap* bannerW;

Lemon::GUI::Label* totalMem;
Lemon::GUI::Label* usedMem;

char versionString[80];

lemon_sysinfo_t sysInfo;

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("System Information", {300, 300}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
    Lemon::Graphics::LoadImage("/initrd/banner.bmp", &banner);
    
    bannerW = new Lemon::GUI::Bitmap({{0, 0}, {300, banner.height}}, &banner);
    window->AddWidget(bannerW);

    int ypos = banner.height + 4;
    sysInfo = lemon_sysinfo();

    char buf[32];

	syscall(SYS_UNAME, versionString,0,0,0,0);

    window->AddWidget(new Lemon::GUI::Label(versionString, {{4, ypos}, {200, 12}}));
    ypos += 24;

    sprintf(buf, "Total System Memory: %d MB (%d KB)", sysInfo.totalMem / 1024, sysInfo.totalMem);
    totalMem = new Lemon::GUI::Label(buf, {{4, ypos}, {200, 12}});
    window->AddWidget(totalMem);
    ypos += 16;
    
    sprintf(buf, "Used System Memory: %d MB (%d KB)", sysInfo.usedMem / 1024, sysInfo.usedMem);
    usedMem = new Lemon::GUI::Label(buf, {{4, ypos}, {200, 12}});
    window->AddWidget(usedMem);
    ypos += 16;

	for(;;){
		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			window->GUIHandleEvent(ev);
		}

		window->Paint();

        usleep(1000);

        lemon_sysinfo_t _sysInfo = lemon_sysinfo();

        if(_sysInfo.usedMem != sysInfo.usedMem){
            sprintf(buf, "Used System Memory: %d MB (%d KB)", sysInfo.usedMem / 1024, sysInfo.usedMem);
            usedMem->label = buf;
        } sysInfo = _sysInfo;
	}
}