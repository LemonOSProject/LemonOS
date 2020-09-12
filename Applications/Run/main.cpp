#include <gui/window.h>
#include <gui/messagebox.h>
#include <lemon/spawn.h>
#include <vector>
#include <cstring>
#include <dirent.h>

std::list<char*> path;

Lemon::GUI::Window* window;
Lemon::GUI::TextBox* textbox;
Lemon::GUI::Button* okButton;

void Run(){
    std::string& text = textbox->contents.front();
    std::vector<char*> args;

    size_t argPos;
    while((argPos = text.find(" ")) != std::string::npos){
        std::string arg = text.substr(0, argPos);
        
        args.push_back(strdup(arg.c_str()));

        text.erase(0, argPos + 1);
    }
    args.push_back(strdup(text.c_str()));

    if(!strchr(args.front(), '/')){
        for(char* p : path){
            DIR* dirp = opendir(p);
            while(auto dent = readdir(dirp)){
                if(!strcmp(dent->d_name, args.front()) || (strstr(dent->d_name, ".lef") && !strncmp(dent->d_name, args.front(), strlen(dent->d_name) - 4))){ // try ommiting any .lef
                    char* execPath = (char*)malloc(strlen(p) + strlen(dent->d_name) + 1); // Path + arg + separator
                    strcpy(execPath, p);
                    strcat(execPath, "/");
                    strcat(execPath, dent->d_name);

                    free(args.front());
                    args.front() = execPath;

                    auto pid = lemon_spawn(args.front(), args.size(), args.data());

                    if(!pid){
                        goto err;
                    } else {
                        delete window;

                        exit(0);
                    }
                }
            }
        }
    } else { // We have been given a path
        auto pid = lemon_spawn(args.front(), args.size(), args.data());

        if(!pid){
            goto err;
        } else {
            delete window;

            exit(0);
        }
    }

err:
    char buf[512];
    snprintf(buf, 512, "Failed to run '%s'", args.front());
    Lemon::GUI::DisplayMessageBox("Failed to run!", buf);

    for(char* s : args){
        free(s);
    }
    return;
}

void OnSubmit(Lemon::GUI::TextBox* textbox){
    Run();
}

void OnPress(Lemon::GUI::Button* button){
    Run();
}

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("Run...", {200, 78}, 0, Lemon::GUI::WindowType::GUI);

    textbox = new Lemon::GUI::TextBox({10, 10, 10, 24}, false);
    textbox->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Fixed);
    textbox->OnSubmit = OnSubmit;

    okButton = new Lemon::GUI::Button("OK", {10, 44, 100, 24});
    okButton->OnPress = OnPress;

    window->AddWidget(textbox);
    window->AddWidget(okButton);

	path.push_back("/initrd");
	path.push_back("/system/bin");

    while(!window->closed){
        Lemon::LemonEvent ev;
        while(window->PollEvent(ev)){
            window->GUIHandleEvent(ev);
        }

        window->Paint();
        window->WaitEvent();
    }

    return 0;
}