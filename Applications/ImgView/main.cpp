#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Widgets.h>
#include <Lemon/GUI/Messagebox.h>
#include <Lemon/GUI/FileDialog.h>

#include <stdio.h>
#include <stdlib.h>

#define IMGVIEW_OPEN 0x1
#define IMGVIEW_SCALING_NONE 0x11
#define IMGVIEW_SCALING_FIT 0x12
#define IMGVIEW_SCALING_FILL 0x13

Lemon::GUI::Window* window;
Lemon::GUI::Image* imgWidget;
Lemon::GUI::WindowMenu fileMenu = { "File", {{.id = IMGVIEW_OPEN, .name = std::string("Open File...")}} };
Lemon::GUI::WindowMenu viewMenu = { "View", {
    { .id = IMGVIEW_SCALING_NONE, .name = std::string("No Scaling for Image") },
    { .id = IMGVIEW_SCALING_FIT, .name = std::string("Fit Image") },
    { .id = IMGVIEW_SCALING_FILL, .name = std::string("Fill Image") }
} };

int LoadImage(char* path){
    if(!path){
        Lemon::GUI::DisplayMessageBox("Image Viewer 2.1", "Invalid Filepath");
        return 1;
    }

    int ret = imgWidget->Load(path);

    if(ret){
        char msg[128];
        sprintf(msg, "Failed to open image, Error Code: %d", ret);
        Lemon::GUI::DisplayMessageBox("Image Viewer 2.1", msg);
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
    } else if(cmd == IMGVIEW_SCALING_NONE){
        imgWidget->SetScaling(Lemon::Graphics::Texture::TextureScaling::ScaleNone);
    } else if(cmd == IMGVIEW_SCALING_FIT){
        imgWidget->SetScaling(Lemon::Graphics::Texture::TextureScaling::ScaleFit);
    } else if(cmd == IMGVIEW_SCALING_FILL){
        imgWidget->SetScaling(Lemon::Graphics::Texture::TextureScaling::ScaleFill);
    }
}

int main(int argc, char** argv){

    imgWidget = new Lemon::GUI::Image({{0, 0}, {0, 0}});
    imgWidget->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);
    imgWidget->SetScaling(Lemon::Graphics::Texture::TextureScaling::ScaleFit);

    if(argc > 1){
        if(LoadImage(argv[1])){
            return -1;
        }
    } else if(LoadImage(Lemon::GUI::FileDialog("."))){
        return -1;
    }
    
    window = new Lemon::GUI::Window("Image Viewer 2.1", {800, 500}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
    window->CreateMenuBar();

    window->menuBar->items.push_back(fileMenu);
    window->menuBar->items.push_back(viewMenu);
	window->OnMenuCmd = OnWindowCmd;

    window->AddWidget(imgWidget);
    
	while(!window->closed){
		Lemon::WindowServer::Instance()->Poll();

		window->GUIPollEvents();

        window->Paint();
        Lemon::WindowServer::Instance()->Wait();
	}
}