#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
//#include <gfx/window/window.h>

#include <stdlib.h>
#include <core/ipc.h>
#include <core/keyboard.h>

#include "list.h"

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

typedef struct {
	uint16_t x;
	uint16_t y;

	uint16_t width; // Width
	uint16_t height; // Height

	uint32_t flags; // Window Flags

	char title[96]; // Title of window

	uint64_t ownerPID;

	handle_t handle;
} __attribute__((packed)) win_info_t;

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

struct Window{
	win_info_t info; // Window Info Structure

	int x; // Window X Position
	int y; // Window Y Position

	int width; // Window Width
	int height; // Window Height

	bool exists; // Checks if the OS still wants the window, will be destroyed if false
};

// Copy one surface to another
void surfacecpy(surface_t* src, surface_t* dest, vector2i_t offset){
	uint32_t* srcBuffer = (uint32_t*)src->buffer;
	uint32_t* destBuffer = (uint32_t*)dest->buffer;
	for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
		for(int j = 0; j < src->width && j < dest->width - offset.x; j++){
			destBuffer[(i+offset.y)*dest->width + j + offset.x] = srcBuffer[i*src->width + j];
		}
	}
}

uint8_t transparency[]{
	1,1,1,255
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

vector2i_t mousePos = {320,240};

int8_t mouseData[3];

int8_t mouseDown = 0;

char testKey = 2;

Window* active;

bool drag = false;

vector2i_t dragOffset;

void DrawWindow(Window* window){
	vector2i_t offset = {window->x,window->y};
	if(!(window->info.flags & WINDOW_FLAGS_NODECORATION)){
		DrawRect(window->x,window->y,1,window->height+25,0,0,0,&surface); // Left border
		DrawRect(window->x + window->width + 1,window->y,1,window->height+26,0,0,0,&surface); // Right border
		DrawRect(window->x,window->y,window->width+1,1,0,0,0,&surface); // Top border
		DrawRect(window->x,window->y + window->height + 25,window->width+2,1,0,0,0,&surface); // Bottom border
		
		if(window == active) DrawRect(window->x+1,window->y+1,window->width,24,WINDOW_COLOUR_ACTIVE_R, WINDOW_COLOUR_ACTIVE_G, WINDOW_COLOUR_ACTIVE_B,&surface); // Title bar
		else DrawRect(window->x+1,window->y+1,window->width,24,WINDOW_COLOUR_INACTIVE_R, WINDOW_COLOUR_INACTIVE_G, WINDOW_COLOUR_INACTIVE_B,&surface); // Title bar
		DrawString("Window", window->x + 4, window->y + 4, 255, 255,255, &surface);
		offset = {window->x+1, window->y+25};
	}
	
	syscall(SYS_COPY_WINDOW_SURFACE, (uint32_t)&surface, (uint32_t)window->info.handle, (uint32_t)&offset, 0, 0);
}

extern "C"
void pmain(){
	handle_t handle;
	syscall(SYS_CREATE_DESKTOP,(uint32_t)&handle,(uint32_t)&surface,0,0,0);
	//syscall(SYS_MEMALLOC, (surface.width*surface.height*4)/4096,(uint32_t)&(surface.buffer),0,0,0);
	surface.buffer = (uint8_t*)malloc(surface.width*surface.height*4);

	if(!surface.buffer) return;
	syscall(SYS_EXEC, (uint32_t)"/shell.lef", 0, 0, 0, 0);

	win_info_t test1;
	test1.width = 320;
	test1.height = 200;
	test1.flags = 0;

	mouseSurface.width = 11;
	mouseSurface.height = 18;
	mouseSurface.buffer = mouseSurfaceBuffer;

	List<Window*> windows;

	syscall(SYS_EXEC, (uint32_t)"/fm.lef", 0, 0, 0, 0);

	unsigned int mouseDev;
	syscall(SYS_OPEN, (uint32_t)"/dev/mouse0", (uint32_t)&mouseDev,0,0,0);

	for(;;){

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

		DrawRect(0,0,surface.width,surface.height,64,128,128,&surface);

		uint32_t windowCount;
		syscall(SYS_DESKTOP_GET_WINDOW_COUNT,(uint32_t)&windowCount,0,0,0,0);

		for(int i = 0; i < windowCount; i++){
			win_info_t window;
			Window* win;
			syscall(SYS_DESKTOP_GET_WINDOW, i, (uint32_t)&window,0,0,0);
			for(int j = 0; j < windows.get_length(); j++){
				if(window.handle == windows[j]->info.handle){
					windows[j]->exists = true;
					windows[j]->info = window;
					goto nextWindow;
				}
			}
			win = (Window*)malloc(sizeof(Window));

			win->info = window;
			win->x = window.x;
			win->y = window.y;
			win->width = window.width;
			win->height = window.height;
			win->exists = true;
			//active = win;
			windows.add_back(win);
		nextWindow:
			continue;
		}
		if((mouseData[0] & 0x1) && !mouseDown){
			mouseDown = true;
			for(int i = windows.get_length()-1; i >= 0; i--){
				Window* win = windows[i];
				if(mousePos.x > win->x && mousePos.x < win->x + win->width && mousePos.y > win->y && mousePos.y < win->y + win->height){
					windows.add_back(windows.remove_at(i));
					active = windows[i];
					if(mousePos.y < win->y + 25 && !(win->info.flags & WINDOW_FLAGS_NODECORATION)){
						dragOffset = {mousePos.x - win->x, mousePos.y - win->y};
						drag = true;
					} else if(win->info.flags & WINDOW_FLAGS_NODECORATION){
						ipc_message_t mouseEventMessage;
						mouseEventMessage.id = WINDOW_EVENT_MOUSEDOWN;
						uint32_t mouseX = mousePos.x - win->x;
						uint16_t mouseY = (mousePos.y - win->y);
						mouseEventMessage.data = (mouseX << 16) + mouseY;
						SendMessage(win->info.ownerPID, mouseEventMessage);
					} else {
						ipc_message_t mouseEventMessage;
						mouseEventMessage.id = WINDOW_EVENT_MOUSEDOWN;
						uint32_t mouseX = mousePos.x - win->x - 1; // Account for window border
						uint16_t mouseY = mousePos.y - win->y - 25; // Account for border and title bar
						mouseEventMessage.data = (mouseX << 16) + mouseY;
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
				ipc_message_t mouseEventMessage;
				mouseEventMessage.id = WINDOW_EVENT_MOUSEUP;
				SendMessage(active->info.ownerPID, mouseEventMessage);
		}

		for(int i = 0; i < windows.get_length();i++){
			if(!windows[i]->exists){ // Window has been destroyed by OS, remove it from the list
				windows.remove_at(i);
			}
			DrawWindow(windows[i]);
			windows[i]->exists = false; // Set exists to false so the window can be checked next frame
		}
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == DESKTOP_EVENT_KEY){
				ipc_message_t keyMsg;
				keyMsg.id = WINDOW_EVENT_KEY;
				keyMsg.data = keymap_us[msg.data];
				SendMessage(active->info.ownerPID, keyMsg);
			}
		}
		surfacecpy_t(&mouseSurface, &surface, mousePos);

		syscall(SYS_PAINT_DESKTOP, (uint32_t)handle, (uint32_t)&surface, 0, 0, 0);
	}
}
