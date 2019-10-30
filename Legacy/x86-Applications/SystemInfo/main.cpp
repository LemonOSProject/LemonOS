#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <fcntl.h>
#include <core/lemon.h>

char uname[64];

uint8_t* bannerBuffer;

void OnWindowPaint(surface_t* surface){
	DrawBitmapImage(0,0,300,80, bannerBuffer, surface);
}

uint64_t lastUptimeSeconds;
uint32_t lastUptimeMs;

uint32_t msCounter;

int waitTime = 1000;

char infoString[512];

void Wait(){
	while(msCounter < waitTime){
		asm("hlt");
		uint32_t seconds;
		uint32_t milliseconds;

		syscall(SYS_UPTIME, (uint32_t)&seconds, (uint32_t)&milliseconds,0,0,0);

		msCounter += (seconds - lastUptimeSeconds)*1000 + (milliseconds - lastUptimeMs);
		lastUptimeSeconds = seconds;
		lastUptimeMs = milliseconds;
	}
	msCounter = 0;
}

void RefreshInfo(){
	lemon_sys_info_t sysInfo;
	lemon_sysinfo(&sysInfo);

	memset(infoString,0,512);

	strcpy(infoString, sysInfo.versionString);
	strcat(infoString, " \nCPU Vendor: ");

	if(strcmp(sysInfo.cpuVendor, "GenuineIntel") == 0) {
		strcat(infoString, "Intel");
	} else if (strcmp(sysInfo.cpuVendor, "AuthenticAMD") == 0){
		strcat(infoString, "AMD");
	} else {
		strcat(infoString, sysInfo.cpuVendor);
	}

	char temp[16];
	itoa(sysInfo.memSize/1024,temp,10);

	strcat(infoString, "\nSystem Memory: ");
	strcat(infoString, temp);
	strcat(infoString, " MB");
	
	itoa(sysInfo.usedMemory/1024,temp,10);

	strcat(infoString, "\nUsed Memory: ");
	strcat(infoString, temp);
	strcat(infoString, " MB");

	uint32_t uptimeSeconds;
	uint32_t uptimeTicks;
	syscall(SYS_UPTIME, (uint32_t)&uptimeSeconds, (uint32_t)&uptimeTicks, 0, 0, 0);

	strcat(infoString, "\nSystem Uptime: ");
	itoa((uptimeSeconds - uptimeSeconds % 60) / 60, temp, 10);
	strcat(infoString, temp);
	strcat(infoString, "m ");
	itoa((uptimeSeconds % 60), temp, 10);
	strcat(infoString, temp);
	strcat(infoString, "s ");
}

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 300;
	windowInfo.height = 300;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	int banner = open("/banner.bmp",0);//fopen("/banner.bmp","r");

	size_t bannerBufferSize = lseek(banner, 0, SEEK_END);//ftell(banner);
	bannerBuffer = (uint8_t*)malloc(bannerBufferSize);
	read(banner, bannerBuffer, bannerBufferSize);

	Window* win = CreateWindow(&windowInfo);
	win->OnPaint = OnWindowPaint;

	//Bitmap* bmp = new Bitmap({{0,0},{300,80}});
	//DrawBitmapImage(0,0,300,80,bannerBuffer, &bmp->surface);
	//win->AddWidget(bmp);

	//fread(bannerBuffer, bannerBufferSize, 1, banner);

	RefreshInfo();
	
	Label* systemInfo = new Label(infoString,{{20,100},{260,300}});
	AddWidget(systemInfo, win);

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch(msg.data){
				default:
					break;
				}
			} else if (msg.id == WINDOW_EVENT_MOUSEUP){
				HandleMouseUp(win);
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;

				HandleMouseDown(win, {mouseX, mouseY});
			}
		}
		//syscall(SYS_PAINT_WINDOW, (uint32_t)win->handle,(uint32_t)&win->surface,0,0,0);
		PaintWindow(win);
		Wait();
		RefreshInfo();
		systemInfo->label = infoString;
	}
}