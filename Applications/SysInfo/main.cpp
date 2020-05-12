#include <gfx/window/window.h>
#include <gfx/window/widgets.h>
#include <gfx/window/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <lemon/ipc.h>
#include <gfx/window/filedialog.h>
#include <lemon/info.h>
#include <unistd.h>

win_info_t winInfo{
    .x = 0,
    .y  = 0,
    .width = 300,
    .height = 300,
    .flags = 0,
};

Lemon::GUI::Window* window;
surface_t banner;
Lemon::GUI::Bitmap* bannerW;

Lemon::GUI::Label* totalMem;
Lemon::GUI::Label* usedMem;

char versionString[80];

lemon_sysinfo_t sysInfo;

int main(int argc, char** argv){
	Lemon::Graphics::InitializeFonts();
    strcpy(winInfo.title, "System Information");

    window = Lemon::GUI::CreateWindow(&winInfo);

    Lemon::Graphics::LoadImage("/initrd/banner.bmp", &banner);
    
    bannerW = new Lemon::GUI::Bitmap({{0, 0}, {300, banner.height}}, &banner);
    window->widgets.add_back(bannerW);

    int ypos = banner.height + 4;
    sysInfo = lemon_sysinfo();

    char buf[32];

	syscall(SYS_UNAME, versionString,0,0,0,0);

    window->widgets.add_back(new Lemon::GUI::Label(versionString, {{4, ypos}, {200, 12}}));
    ypos += 24;

    sprintf(buf, "Total System Memory: %d MB (%d KB)", sysInfo.totalMem / 1024, sysInfo.totalMem);
    totalMem = new Lemon::GUI::Label(buf, {{4, ypos}, {200, 12}});
    window->widgets.add_back(totalMem);
    ypos += 16;
    
    sprintf(buf, "Used System Memory: %d MB (%d KB)", sysInfo.usedMem / 1024, sysInfo.usedMem);
    usedMem = new Lemon::GUI::Label(buf, {{4, ypos}, {200, 12}});
    window->widgets.add_back(usedMem);
    ypos += 16;

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
			} else if (msg.msg == WINDOW_EVENT_KEY) {
				Lemon::GUI::HandleKeyPress(window, msg.data);
			} else if (msg.msg == WINDOW_EVENT_CLOSE) {
				Lemon::GUI::DestroyWindow(window);
				exit(0);
			}
		}

		Lemon::GUI::PaintWindow(window);

        usleep(1000);

        lemon_sysinfo_t _sysInfo = lemon_sysinfo();

        if(_sysInfo.usedMem != _sysInfo.totalMem){
            sprintf(buf, "Used System Memory: %d MB (%d KB)", sysInfo.usedMem / 1024, sysInfo.usedMem);
            usedMem->label = buf;
        } sysInfo = _sysInfo;
	}
}