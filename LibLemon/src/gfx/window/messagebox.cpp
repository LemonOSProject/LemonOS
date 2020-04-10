#include <gfx/window/window.h>
#include <lemon/ipc.h>
#include <gfx/window/widgets.h>
#include <gfx/window/messagebox.h>

int MessageBox(char* messageString, int flags){
	return 0;
	win_info_t msgBoxWindowInfo;
	msgBoxWindowInfo.x = 50;
	msgBoxWindowInfo.y = 50;
	if(strlen(messageString)*8 < 400)
		msgBoxWindowInfo.width = 400;
	else
		msgBoxWindowInfo.width = strlen(messageString)*8 + 100;
	msgBoxWindowInfo.height = 100;
	msgBoxWindowInfo.flags = 0;

	Window* win = CreateWindow(&msgBoxWindowInfo);

	Label* message = new Label(messageString, {{msgBoxWindowInfo.width / 2 - strlen(messageString) * 8 / 2,50},{strlen(messageString)*8,12}});

	Button* btn1;
	Button* btn2;
	Button* btn3;

	if(flags & MESSAGEBOX_OK){
		btn1 = new Button("OK",{{msgBoxWindowInfo.width/2-50,msgBoxWindowInfo.height - 30},{100,24}});
		btn1->style = 1;

		AddWidget(btn1, win);
	} else if(flags & MESSAGEBOX_OKCANCEL){
		btn1 = new Button("OK",{{msgBoxWindowInfo.width/2-105,msgBoxWindowInfo.height - 30},{100,24}});
		btn1->style = 1;
		AddWidget(btn1, win);

		btn2 = new Button("Cancel",{{msgBoxWindowInfo.width/2+5,msgBoxWindowInfo.height - 30},{100,24}});
		btn2->style = 2;
		AddWidget(btn2, win);
	} else if(flags & MESSAGEBOX_EXITRETRYIGNORE){
		btn1 = new Button("Exit",{{msgBoxWindowInfo.width/2-155,msgBoxWindowInfo.height - 30},{100,24}});
		btn1->style = 2;
		AddWidget(btn1, win);

		btn2 = new Button("Retry",{{msgBoxWindowInfo.width/2-50,msgBoxWindowInfo.height - 30},{100,24}});
		btn2->style = 3;
		AddWidget(btn2, win);

		btn3 = new Button("Ignore",{{msgBoxWindowInfo.width/2+55,msgBoxWindowInfo.height - 30},{100,24}});
		btn3->style = 0;
		AddWidget(btn3, win);
	}

	AddWidget(message, win);

	PaintWindow(win);
	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				Widget* widget;
				if((widget = HandleMouseUp(win, {mouseX, mouseY}))){
					if(flags & MESSAGEBOX_OK){
						if(widget == btn1) {
							DestroyWindow(win);
							return 0;
						}
					} else if(flags & MESSAGEBOX_OKCANCEL){
						if(widget == btn1) {
							DestroyWindow(win);
							return 0;
						}
						else if (widget == btn2) {
							DestroyWindow(win);
							return 1;
						}
					} else if(flags & MESSAGEBOX_EXITRETRYIGNORE){
						if(widget == btn1) {
							DestroyWindow(win);
							return 1;
						}
						else if (widget == btn2) {
							DestroyWindow(win);
							return 2;
						}
						else if (widget == btn3) {
							DestroyWindow(win);
							return 0;
						}
					}
				}
			} else if (msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseDown(win, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_CLOSE){
				return 0;
			}
		}
	}
}