#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>

#include <stdlib.h>
#include <core/ipc.h>
#include <core/keyboard.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

int keymap_us[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	0,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   0,		/* Left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
	0,	/* Alt */
	' ',	/* Space bar */
	0,	/* Caps lock */
	KEY_F1,	/* 59 - F1 key ... > */
	KEY_F2,   KEY_F3,   KEY_F4,   KEY_F5,   KEY_F6,   KEY_F7,   KEY_F8,   KEY_F9,
	KEY_F10,	/* < ... F10 */
	0,	/* 69 - Num lock*/
	0,	/* Scroll Lock */
	0,	/* Home key */
	KEY_ARROW_UP,	/* Up Arrow */
	0,	/* Page Up */
	'-',
	KEY_ARROW_LEFT,	/* Left Arrow */
	0,
	KEY_ARROW_RIGHT,	/* Right Arrow */
	'+',
	0,	/* 79 - End key*/
	KEY_ARROW_DOWN,	/* Down Arrow */
	0,	/* Page Down */
	0,	/* Insert Key */
	0,	/* Delete Key */
	0,   0,   0,
	0,	/* F11 Key */
	0,	/* F12 Key */
	0,	/* All other keys are undefined */
};

#define WINDOW_FLAGS_NODECORATION 0x1
#define WINDOW_FLAGS_SNAP_TO_BOTTOM 0x2

void *operator new(size_t size)
{
	return malloc(size);
}

void *operator new[](size_t size)
{
	return malloc(size);
}

void operator delete(void *p)
{
	free(p);
}

void operator delete(void *p, size_t)
{
	::operator delete(p);
}


void operator delete[](void *p)
{
	free(p);
}

void operator delete[](void *p, size_t)
{
	::operator delete[](p);
}

struct Window_s{
	win_info_t info; // Window Info Structure

	int x; // Window X Position
	int y; // Window Y Position

	int width; // Window Width
	int height; // Window Height

	bool exists; // Checks if the OS still wants the window, will be destroyed if false
};

// surfacecpy with transparency
void surfacecpy_t(surface_t* src, surface_t* dest, vector2i_t offset){
	uint32_t* srcBuffer = (uint32_t*)src->buffer;
	uint32_t* destBuffer = (uint32_t*)dest->buffer;
	for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
		for(int j = 0; j < src->width && j < dest->width - offset.x; j++){
			if((srcBuffer[i*src->width + j] >> 24) < 255) continue;
			destBuffer[(i+offset.y)*dest->width + j + offset.x] = srcBuffer[i*src->width + j];
		}
	}
}

List<rect_t>* SplitRectangle(rect_t subject, rect_t cut){
	List<rect_t>* splittedRects = new List<rect_t>();

	// Cut by left edge
	if(cut.pos.x >= subject.pos.x && cut.pos.x <= subject.pos.x + subject.size.x){
		rect_t newRect;
		newRect.pos.x = subject.pos.x;
		newRect.pos.y = subject.pos.y;
		newRect.size.x = cut.pos.x - subject.pos.x - 1;
		newRect.size.y = subject.size.y;

		splittedRects->add_back(newRect);

		subject.pos.x = cut.pos.x;
	}

	// Cut by top edge
	if(cut.pos.y >= subject.pos.y && cut.pos.y <= subject.pos.y + subject.size.y){
		rect_t newRect;
		newRect.pos.x = subject.pos.x;
		newRect.pos.y = subject.pos.y;
		newRect.size.x = subject.size.x;
		newRect.size.y = cut.pos.y - subject.pos.y - 1;

		splittedRects->add_back(newRect);

		subject.pos.y = cut.pos.y;
	}

	// Cut by right edge
	if(cut.pos.x + cut.size.x >= subject.pos.x && cut.pos.x + cut.size.x <= subject.pos.x + subject.size.x){
		rect_t newRect;
		newRect.pos.x = cut.pos.x + cut.size.x;
		newRect.pos.y = subject.pos.y;
		newRect.size.x = subject.size.x - (newRect.pos.x - subject.pos.x);
		newRect.size.y = subject.size.y;

		splittedRects->add_back(newRect);

		subject.size.x = (cut.pos.x + cut.size.x) - subject.pos.x;
	}

	// Cut by bottom edge
	if(cut.pos.y + cut.size.y >= subject.pos.y && cut.pos.y <= subject.pos.y + subject.size.y){
		rect_t newRect;
		newRect.pos.y = cut.pos.y + cut.size.y;
		newRect.pos.x = subject.pos.x;
		newRect.size.y = subject.size.y - (newRect.pos.y - subject.pos.y);
		newRect.size.x = subject.size.x;

		splittedRects->add_back(newRect);
		
		subject.size.y = (cut.pos.y + cut.size.y) - subject.pos.y;
	}
	return splittedRects;
}

surface_t surface;
surface_t mouseSurface;

#define pW 255,255,255,255 // White
#define pG 128,128,128,255 // Gray
#define pB 0,0,0,255 // Black
#define pT 1,1,1,0 // Transparent

uint8_t mouseSurfaceBuffer[11 * 18 * 4]{ /* Width: 11, Height: 18 */
	pB,pT,pT,pT,pT,pT,pT,pT,pT,pT,pT,
	pB,pB,pT,pT,pT,pT,pT,pT,pT,pT,pT,
	pB,pG,pB,pT,pT,pT,pT,pT,pT,pT,pT,
	pB,pW,pG,pB,pT,pT,pT,pT,pT,pT,pT,
	pB,pW,pW,pG,pB,pT,pT,pT,pT,pT,pT,
	pB,pW,pW,pW,pG,pB,pT,pT,pT,pT,pT,
	pB,pW,pW,pW,pW,pG,pB,pT,pT,pT,pT,
	pB,pW,pW,pW,pW,pW,pG,pB,pT,pT,pT,
	pB,pW,pW,pW,pW,pW,pW,pG,pB,pT,pT,
	pB,pW,pW,pW,pW,pW,pW,pW,pG,pB,pT,
	pB,pW,pW,pW,pW,pW,pW,pW,pW,pG,pB,
	pB,pW,pW,pW,pW,pW,pB,pB,pB,pB,pB,
	pB,pW,pG,pB,pG,pW,pB,pG,pT,pT,pT,
	pB,pG,pB,pT,pB,pW,pW,pB,pT,pT,pT,
	pB,pB,pT,pT,pB,pW,pW,pB,pG,pT,pT,
	pT,pT,pT,pT,pG,pB,pW,pW,pB,pT,pT,
	pT,pT,pT,pT,pT,pB,pW,pW,pB,pT,pT,
	pT,pT,pT,pT,pT,pG,pB,pB,pG,pT,pT
};

#define WINDOW_COLOUR_ACTIVE_R 48
#define WINDOW_COLOUR_ACTIVE_G 48
#define WINDOW_COLOUR_ACTIVE_B 48
#define WINDOW_COLOUR_INACTIVE_R 128
#define WINDOW_COLOUR_INACTIVE_G 128
#define WINDOW_COLOUR_INACTIVE_B 128

#define ENABLE_FRAMERATE_COUNTER

vector2i_t mousePos = {320,240};

int8_t mouseData[3];

int8_t mouseDown = 0;

char testKey = 2;

Window_s* active;

bool drag = false;

vector2i_t dragOffset;

surface_t closeButtonSurface;

bool redrawWindowDecorations = true;

void DrawWindow(Window_s* window){
	vector2i_t offset = {window->x,window->y};
	rect_t windowRegion = {{0,0},{window->width, window->height}};
	if(!(window->info.flags & WINDOW_FLAGS_NODECORATION) && redrawWindowDecorations){
		DrawRect(window->x,window->y,1,window->height+25,0,0,0,&surface); // Left border
		DrawRect(window->x + window->width + 1,window->y,1,window->height+26,0,0,0,&surface); // Right border
		DrawRect(window->x,window->y,window->width+1,1,0,0,0,&surface); // Top border
		DrawRect(window->x,window->y + window->height + 25,window->width+2,1,0,0,0,&surface); // Bottom border
		
		//if(window == active) DrawRect(window->x+1,window->y+1,window->width,24,WINDOW_COLOUR_ACTIVE_R, WINDOW_COLOUR_ACTIVE_G, WINDOW_COLOUR_ACTIVE_B,&surface); // Title bar
		//else DrawRect(window->x+1,window->y+1,window->width,24,WINDOW_COLOUR_INACTIVE_R, WINDOW_COLOUR_INACTIVE_G, WINDOW_COLOUR_INACTIVE_B,&surface); // Title bar
		if(window == active) DrawGradient(window->x+1, window->y+1,window->width,24,{32,32,32,255},{96,96,96,255},&surface);
		else DrawRect(window->x+1,window->y+1,window->width,24,WINDOW_COLOUR_INACTIVE_R, WINDOW_COLOUR_INACTIVE_G, WINDOW_COLOUR_INACTIVE_B,&surface); // Title bar

		DrawString(window->info.title, window->x + 4, window->y + 4, 255, 255,255, &surface);
		offset = {window->x+1, window->y+25};

		DrawRect(window->x + window->width - 21, window->y + 3, 1, 18, 100, 50, 50, &surface);
		DrawRect(window->x + window->width - 3, window->y + 3, 1, 18, 100, 50, 50, &surface);
		DrawRect(window->x + window->width - 21, window->y + 3, 18, 1, 100, 50, 50, &surface);
		DrawRect(window->x + window->width - 21, window->y + 21, 18, 1, 100, 50, 50, &surface);
		surfacecpy(&surface, &closeButtonSurface, {window->x + window->width - 20, window->y + 4});
	}
	syscall(SYS_COPY_WINDOW_SURFACE, (uint32_t)&surface, (uint32_t)window->info.handle, (uint32_t)&offset, (uint32_t)&windowRegion, 0);
}

#ifdef ENABLE_FRAMERATE_COUNTER

size_t frameCounter;

uint32_t lastUptimeSeconds;
uint32_t lastUptimeMilliseconds;

int frameRate;

uint32_t currentUptimeSeconds;
uint32_t currentUptimeMilliseconds;

void UpdateFrameRate(){
	syscall(SYS_UPTIME, (uint32_t)&currentUptimeSeconds, (uint32_t)&currentUptimeMilliseconds, 0, 0, 0);
	uint32_t difference = currentUptimeSeconds*1000 - lastUptimeSeconds*1000 - lastUptimeMilliseconds + currentUptimeMilliseconds;
	if(difference >= 5000){
		frameRate = frameCounter/5;
		frameCounter = 0;
		lastUptimeSeconds = currentUptimeSeconds;
		lastUptimeMilliseconds = currentUptimeMilliseconds;
	}
}
#endif

surface_t bgSurface;

List<rect_t>* bgClipRects = NULL;

void RecalculateBackgroundClipRects(List<Window_s*>* windowList){
	return;
	rect_t bgRect = {{0,0},{surface.width,surface.height}};
	//delete bgClipRects;
	//bgClipRects = new List<rect_t>();
	bgClipRects->add_back(bgRect);
	for(int i = 0; i < windowList->get_length();i++){
		Window_s win = *windowList->get_at(i);
		reloop:
		for(int j = 0; j < bgClipRects->get_length();){
			rect_t tempRect = bgClipRects->get_at(j);
			if(!(tempRect.pos.x <= win.x + win.width && tempRect.pos.x + tempRect.size.x >= win.x && tempRect.pos.y <= win.y + win.height && tempRect.pos.y + tempRect.size.y >= win.y)){
				j++;
				continue;
			}

			bgClipRects->remove_at(j);
			List<rect_t>* tempClipRects = SplitRectangle(tempRect,{{win.x,win.y},{win.width,win.height}});
			//while(tempClipRects->get_length() > 0){
			for(int k = 0; k < tempClipRects->get_length(); k++)
				bgClipRects->add_back(tempClipRects->get_at(k));
				
			delete tempClipRects;
			goto reloop;
		}
	}
}
	
int main(){
	handle_t handle;
	syscall(SYS_CREATE_DESKTOP,(uint32_t)&handle,(uint32_t)&surface,0,0,0);
	surface.buffer = (uint8_t*)malloc(surface.width*surface.height*4);

	if(!surface.buffer) for(;;);
	//syscall(SYS_EXEC, (uint32_t)"/terminal.lef", 0, 0, 0, 0);

	win_info_t test1;
	test1.width = 320;
	test1.height = 200;
	test1.flags = 0;

	mouseSurface.width = 11;
	mouseSurface.height = 18;
	mouseSurface.buffer = mouseSurfaceBuffer;
	
	bgClipRects = new List<rect_t>();

	List<Window_s*> windows;

	syscall(SYS_EXEC, (uint32_t)"/taskbar.lef", 0, 0, 0, 0);

	unsigned int mouseDev;
	syscall(SYS_OPEN, (uint32_t)"/dev/mouse0", (uint32_t)&mouseDev,0,0,0);

	closeButtonSurface.width = 16;
	closeButtonSurface.height = 16;

	int closeButtonFile = open("/close.bmp", 0);
	closeButtonSurface.buffer = (uint8_t*)malloc(16*16*4);
	int closeButtonBMPSize = lseek(closeButtonFile,0,SEEK_END);
	uint8_t* closeButtonBMPBuffer = (uint8_t*)malloc(closeButtonBMPSize);
	read(closeButtonFile, closeButtonBMPBuffer, closeButtonBMPSize);
	DrawBitmapImage(0, 0, 16, 16, closeButtonBMPBuffer, &closeButtonSurface);
	close(closeButtonFile);

	int backgroundFile = open("/bg1.bmp",0);
	int backgroundFileSize = lseek(backgroundFile,0,SEEK_END);
	uint8_t* backgroundBuffer = (uint8_t*)malloc(backgroundFileSize);
	read(backgroundFile,backgroundBuffer,backgroundFileSize);
	
	bgSurface.width = 1024;
	bgSurface.height = 768;
	bgSurface.buffer = (uint8_t*)malloc(1024*768*4);
	
	if(backgroundFile)
		DrawBitmapImage(0,0,1024,768,backgroundBuffer,&bgSurface);

	RecalculateBackgroundClipRects(&windows);

	for(;;){
		#ifdef ENABLE_FRAMERATE_COUNTER
		frameCounter++;
		UpdateFrameRate();
		#endif

		uint32_t mouseReturn;
		syscall(SYS_READ, mouseDev, (uint32_t)mouseData,3,(uint32_t)&mouseReturn,0);
		
		if(mouseReturn){
			mousePos.x += mouseData[1] * 2;
			if(mousePos.x < 0) mousePos.x = 0;
			mousePos.y += -mouseData[2] * 2;
			if(mousePos.y < 0) mousePos.y = 0;
			//mouseDown = mouseData[0] & 0x1;
		}

		if(drag){
			active->x = mousePos.x - dragOffset.x;
			active->y = mousePos.y - dragOffset.y;
			if(active->x < 0) active->x = 0;
			if(active->y < 0) active->y = 0;
		}
		//if(backgroundFile)
		//	surfacecpy(&surface, &bgSurface, {0,0});
		//else

		//if(redrawWindowDecorations)

		redrawWindowDecorations = true;

		uint32_t windowCount;
		syscall(SYS_DESKTOP_GET_WINDOW_COUNT,(uint32_t)&windowCount,0,0,0,0);

		for(int i = 0; i < windowCount; i++){
			Window_s* win;
			win_info_t window;
			syscall(SYS_DESKTOP_GET_WINDOW, i, (uint32_t)&window,0,0,0);
	
			for(int j = 0; j < windows.get_length(); j++){
				if(window.handle == windows[j]->info.handle){
					windows[j]->exists = true;
					windows[j]->info = window;
					goto nextWindow;
				}
			}
			win = (Window_s*)malloc(sizeof(Window_s));

			win->info = window;
			win->x = window.x;
			win->y = window.y;
			win->width = window.width;
			win->height = window.height;
			win->exists = true;
			//strcpy(win->info.title, window.title);
			
			active = win;
			windows.add_back(win);

			redrawWindowDecorations = true;
			RecalculateBackgroundClipRects(&windows);
		nextWindow:
			continue;
		}

		if(mouseDown) RecalculateBackgroundClipRects(&windows);

		if((mouseData[0] & 0x1) && !mouseDown){
			mouseDown = true;
			for(int i = windows.get_length()-1; i >= 0; i--){
				Window_s* win = windows[i];
				if(mousePos.x > win->x && mousePos.x < win->x + win->width && mousePos.y > win->y && mousePos.y < win->y + win->height){
					windows.add_back(windows.remove_at(i));
					active = windows[i];
					redrawWindowDecorations = true;
					if(mousePos.y < win->y + 25 && !(win->info.flags & WINDOW_FLAGS_NODECORATION)){
						if(mousePos.x < win->x + win->width - 4 && mousePos.y < win->y + 20 && mousePos.x > win->x + win->width - 20 && mousePos.y > win->y + 4){
							ipc_message_t closeEventMessage;
							closeEventMessage.id = WINDOW_EVENT_CLOSE;
							closeEventMessage.data2 = (uint32_t)win->info.handle;
							SendMessage(win->info.ownerPID, closeEventMessage);
						} else {
							dragOffset = {mousePos.x - win->x, mousePos.y - win->y};
							drag = true;
						}
					} else {
						ipc_message_t mouseEventMessage;
						mouseEventMessage.id = WINDOW_EVENT_MOUSEDOWN;
						uint32_t mouseX;
						uint16_t mouseY;
						if(win->info.flags & WINDOW_FLAGS_NODECORATION){
							mouseX = mousePos.x - win->x;
							mouseY = mousePos.y - win->y;
						} else {
							mouseX = mousePos.x - win->x - 1; // Account for window border
							mouseY = mousePos.y - win->y - 25; // Account for border and title bar
						}
						mouseEventMessage.data = (mouseX << 16) + mouseY;
						mouseEventMessage.data2 = (uint32_t)win->info.handle;
						SendMessage(win->info.ownerPID, mouseEventMessage);
					}
					break;
				}
			}
		} else if (!(mouseDown) && drag){
			drag = false;
		}

		if(mouseDown && !(mouseData[0] & 0x1)){
			mouseDown = false;
			drag = false;
			ipc_message_t mouseEventMessage;
			mouseEventMessage.id = WINDOW_EVENT_MOUSEUP;
			if(active)
				SendMessage(active->info.ownerPID, mouseEventMessage);
		}

		DrawRect(0,0,surface.width,surface.height - 24 /*As a minor performance improvement don't draw where taskbar is*/,64,128,128,&surface);

		//for(int i; i < bgClipRects->get_length(); i++){
		//	rect_t rect = bgClipRects->get_at(i);
			//DrawRect(rect,{64,128,128,255},&surface);
		//}
/*
		for(int i = 0; i < bgClipRects->get_length();i++){
			//continue;
			rect_t rect = bgClipRects->get_at(i);
			if(mousePos.x > rect.pos.x && mousePos.x < rect.pos.x + rect.size.x && mousePos.y > rect.pos.y && mousePos.y < rect.pos.y + rect.size.y);
				DrawRect(rect,{64,128,/*128* /rand() % 255,255},&surface);
}*/

		//delete bgClipRects;

		reloop1:
		for(int i = 0; i < windows.get_length();i++){
			if(!windows[i]->exists){ // Window has been destroyed by OS, remove it from the list
				if(windows[i] == active) active = NULL;
				windows.remove_at(i);
				i = 0;
				RecalculateBackgroundClipRects(&windows);
				continue;
			}

			DrawWindow(windows[i]);
			windows[i]->exists = false; // Set exists to false so the window can be checked next frame
		}
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == DESKTOP_EVENT_KEY && active){
				ipc_message_t keyMsg;
				if((msg.data >> 7) && (msg.data & 0x7F)) {
					keyMsg.id = WINDOW_EVENT_KEYRELEASED;
					keyMsg.data = keymap_us[msg.data & 0x7F];
				} else {
					keyMsg.id = WINDOW_EVENT_KEY;
					keyMsg.data = keymap_us[msg.data];
				}
				keyMsg.data2 = (uint32_t)active->info.handle;
				SendMessage(active->info.ownerPID, keyMsg);
			}
		}
		surfacecpy_t(&mouseSurface, &surface, mousePos);

		#ifdef ENABLE_FRAMERATE_COUNTER
		DrawRect(0,0,8*3,12,0,0,0,&surface);
		char temp[16];
		itoa(frameRate, temp, 10);
		DrawString(temp,0,0,255,255,255,&surface);
		#endif

		syscall(SYS_PAINT_DESKTOP, (uint32_t)handle, (uint32_t)&surface, 0, 0, 0);
	}
}