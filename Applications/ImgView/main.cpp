#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Widgets.h>
#include <Lemon/GUI/Messagebox.h>
#include <Lemon/GUI/FileDialog.h>

#include <stdio.h>
#include <stdlib.h>

#define IMGVIEW_OPEN 1

Lemon::GUI::Window* window;
Lemon::GUI::Image* imgWidget;
Lemon::GUI::WindowMenu fileMenu;

int LoadImage(char* path){
    if(!path){
        Lemon::GUI::DisplayMessageBox("Image Viewer", "Invalid Filepath");
        return 1;
    }

    int ret = imgWidget->Load(path);

    if(ret){
        char msg[128];
        sprintf(msg, "Failed to open image, Error Code: %d", ret);
        Lemon::GUI::DisplayMessageBox("Image Viewer", msg);
        return ret;
    }

    return 0;
}

void OnWindowCmd(unsigned short cmd, Lemon::GUI::Window* win){
    if(cmd == IMGVIEW_OPEN){
        if(LoadImage(Lemon::GUI::FileDialog("/"))){
            delete win;

            exit(-1);
        }
    }
}

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("Image Viewer", {800, 500}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
    window->CreateMenuBar();

    fileMenu.first = "File";
	fileMenu.second.push_back({.id = IMGVIEW_OPEN, .name = std::string("Open...")});

    window->menuBar->items.push_back(fileMenu);
	window->OnMenuCmd = OnWindowCmd;

    imgWidget = new Lemon::GUI::Image({{0, 0}, {0, 0}});
    imgWidget->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);

    window->AddWidget(imgWidget);

    if(argc > 1){
        if(LoadImage(argv[1])){
            return -1;
        }
    } else if(LoadImage(Lemon::GUI::FileDialog("."))){
        return -1;
    }
    
	while(!window->closed){
        Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
            window->GUIHandleEvent(ev);
        }

        window->Paint();
        window->WaitEvent();
	}
}