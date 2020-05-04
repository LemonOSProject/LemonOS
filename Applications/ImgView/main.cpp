#include <gfx/window/window.h>
#include <gfx/window/widgets.h>
#include <gfx/window/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <lemon/ipc.h>
#include <gfx/window/filedialog.h>

win_info_t winInfo{
    .x = 0,
    .y  = 0,
    .width = 640,
    .height = 480,
    .flags = 0,
};

Lemon::GUI::Window* window;
Lemon::GUI::ScrollView* sv;
Lemon::GUI::Bitmap* imgWidget;
surface_t image;

int LoadImage(char* path){
    if(!path){
        Lemon::GUI::MessageBox("Invalid Filepath", MESSAGEBOX_OK);
        return 1;
    }

    int ret = Lemon::Graphics::LoadImage(path, &image);

    if(ret){
        char msg[128];
        sprintf(msg, "Failed to open image, Error Code: %d", ret);
        Lemon::GUI::MessageBox(msg, MESSAGEBOX_OK);
        return ret;
    }

    return 0;
}

void OnOpen(Lemon::GUI::Button* btn){
    LoadImage(Lemon::GUI::FileDialog("/initrd"));
}


int main(int argc, char** argv){
    strcpy(winInfo.title, "Image Viewer");

    int ret;
    if(argc > 1){
        ret = LoadImage(argv[1]);
    }
    
retry:
    if (argc < 2 || ret) {
        if(LoadImage(Lemon::GUI::FileDialog("."))) goto retry;
    }

    winInfo.width = image.width + 16;
    winInfo.height = image.height + 16;

    if(image.width < 100) winInfo.width = 100;
    if(image.height < 100) winInfo.height = 100;
    if(image.width > 640) winInfo.width = 640;
    if(image.height > 480) winInfo.height = 480;

    window = Lemon::GUI::CreateWindow(&winInfo);

    sv = new Lemon::GUI::ScrollView({{0, 0}, {winInfo.width, winInfo.height}});
    imgWidget = new Lemon::GUI::Bitmap({{0, 0}, {0, 0}}, &image);

    sv->AddWidget(imgWidget);

    window->widgets.add_back(sv);
    
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
	}
}