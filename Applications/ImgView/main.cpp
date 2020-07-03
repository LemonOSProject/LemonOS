#include <gui/window.h>
#include <gui/widgets.h>
#include <gui/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <lemon/ipc.h>
#include <gui/filedialog.h>

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
        Lemon::GUI::DisplayMessageBox("Image Viewer", "Invalid Filepath");
        return 1;
    }

    int ret = Lemon::Graphics::LoadImage(path, &image);

    if(ret){
        char msg[128];
        sprintf(msg, "Failed to open image, Error Code: %d", ret);
        Lemon::GUI::DisplayMessageBox("Image Viewer", msg);
        return ret;
    }

    return 0;
}

void OnOpen(Lemon::GUI::Button* btn){
    LoadImage(Lemon::GUI::FileDialog("/"));
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

    window = new Lemon::GUI::Window("Image Viewer", {800, 500}, 0, Lemon::GUI::WindowType::GUI);

    sv = new Lemon::GUI::ScrollView({{0, 0}, {winInfo.width, winInfo.height}});
    imgWidget = new Lemon::GUI::Bitmap({{0, 0}, {0, 0}}, &image);

    sv->AddWidget(imgWidget);

    window->AddWidget(sv);
    
	while(!window->closed){
        Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
            window->GUIHandleEvent(ev);
        }
	}
}