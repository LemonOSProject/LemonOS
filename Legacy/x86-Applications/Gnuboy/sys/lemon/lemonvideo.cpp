#include <gfx/window/window.h>
#include <core/ipc.h>
#include <gfx/window/filedialog.h>

#include "fb.h"
#include "rc.h"

Window* win;
win_info_t windowInfo;
struct fb fb;

rcvar_t vid_exports[] =
{
	//RCV_VECTOR("vmode", &vmode, 3),
	//RCV_INT("fb_depth", &fb_depth),
	//RCV_BOOL("yuv", &use_yuv),
	//RCV_BOOL("yuvinterp", &use_interp),
	RCV_END
};

rcvar_t pcm_exports[] =
{
	RCV_END
};

rcvar_t joy_exports[] =
{
	RCV_END
};

void OnPaint(surface_t* surface){
	for(int i = 0; i < 160; i++){
		for(int j = 0; j < 144; j++){
			DrawRect(i*2, j*2, 2, 2, fb.ptr[j*160*3 + i*3], fb.ptr[j*160*3 + i*3 + 1], fb.ptr[j*160*3 + i*3 + 2], surface);
		}
	}
}

extern "C"{

	char* fileDialog(){
		return FileDialog("/");
	}

	void vid_preinit(){
		windowInfo.x = 100;
		windowInfo.y = 100;
		windowInfo.width = 160*2;
		windowInfo.height = 144*2;
		windowInfo.flags = 0;
		strcpy(windowInfo.title, "Gnuboy");

		fb.w = 160;
		fb.h = 144;

		fb.enabled = 1;
		fb.dirty = 0;

		fb.yuv = 0;
		fb.pelsize = 3;
		fb.pitch = 160*3;
		fb.indexed = 0;
		fb.cc[0].r = fb.cc[1].r = fb.cc[2].r = 0;
		fb.cc[0].l = 0;
		fb.cc[1].l = 8;
		fb.cc[2].l = 16;

		fb.ptr = (byte*)malloc(160*144*4);

		win = CreateWindow(&windowInfo);
		win->OnPaint = OnPaint;
	}

	void vid_init(){
	}

	void vid_settitle(char* title){

	}

	void vid_begin(){
		PaintWindow(win);
	}

	void vid_end(){

	}

	void vid_setpal(){

	}

	#include "defs.h"
	#include "input.h"
	#include "hw.h"


	void ev_poll(){
		ipc_message_t msg;
		event_t ev;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				switch((char)msg.data){
					case 'm':
						pad_set(PAD_A, 1);
						break;
					case 'n':
						pad_set(PAD_B, 1);
						break;
					case 'w':
						pad_set(PAD_UP, 1);
						break;
					case 'd':
						pad_set(PAD_RIGHT, 1);
						break;
					case 's':
						pad_set(PAD_DOWN, 1);
						break;
					case 'a':
						pad_set(PAD_LEFT, 1);
						break;
					case '.':
						pad_set(PAD_SELECT, 1);
						break;
					case ',':
						pad_set(PAD_START, 1);
						break;
				}
			} else if(msg.id == WINDOW_EVENT_KEYRELEASED){
				switch((char)msg.data){
					case 'm':
						pad_set(PAD_A, 0);
						break;
					case 'n':
						pad_set(PAD_B, 0);
						break;
					case 'w':
						pad_set(PAD_UP, 0);
						break;
					case 'd':
						pad_set(PAD_RIGHT, 0);
						break;
					case 's':
						pad_set(PAD_DOWN, 0);
						break;
					case 'a':
						pad_set(PAD_LEFT, 0);
						break;
					case '.':
						pad_set(PAD_SELECT, 0);
						break;
					case ',':
						pad_set(PAD_START, 0);
						break;
				}
			} else if (msg.id == WINDOW_EVENT_CLOSE){
				_DestroyWindow(win->handle);
				exit();
			}
		}
	}
}
