#include <gui/shell.h>
#include <core/msghandler.h>
#include <string.h>

#include "shell.h"

ShellInstance::ShellInstance(Lemon::GUI::Window* taskbar, Lemon::GUI::Window* menu){
	sockaddr_un shellSrvAddr;
	strcpy(shellSrvAddr.sun_path, Lemon::GUI::shellSocketAddress);
	shellSrvAddr.sun_family = AF_UNIX;
	shellSrv = Lemon::MessageServer(shellSrvAddr, sizeof(sockaddr_un));

    this->taskbar = taskbar;
    this->menu = menu;
}

void ShellInstance::PollCommands(){
    while(auto m = shellSrv.Poll()){
        if(m->msg.protocol == 0){ // Disconnected
            continue;
        }

        if(m->msg.protocol == LEMON_MESSAGE_PROTOCOL_SHELLCMD){
            Lemon::GUI::ShellCommand* cmd = (Lemon::GUI::ShellCommand*)m->msg.data;

            switch (cmd->cmd)
            {
            case Lemon::GUI::LemonShellAddWindow:
                {
                    char* title = (char*)malloc(cmd->titleLength + 1);
                    strncpy(title, cmd->windowTitle, cmd->titleLength);
                    title[cmd->titleLength] = 0; // Null terminate
                }
                break;
            case Lemon::GUI::LemonShellRemoveWindow:
                ShellWindow* win;
                try{
                    win = windows.at(cmd->windowID);
                } catch (std::out_of_range e){
                    printf("Warning: Shell: LemonShellSetActive: Window ID out of range");
                    break;
                }

                windows.erase(cmd->windowID);
                break;
            case Lemon::GUI::LemonShellToggleMenu:
                showMenu = !showMenu;
                menu->Minimize(!showMenu);
                break;
            case Lemon::GUI::LemonShellSetActive:
                ShellWindow* win;
                try{
                    win = windows.at(cmd->windowID);
                } catch (std::out_of_range e){
                    printf("Warning: Shell: LemonShellSetActive: Window ID out of range");
                    break;
                }

                if(active){
                    active->state = Lemon::GUI::ShellWindowStateNormal;
                }

                active = win;
                break;
            case Lemon::GUI::LemonShellOpen:
                char* path = (char*)malloc(cmd->pathLength + 1);

                strncpy(path, cmd->path, cmd->pathLength);
                path[cmd->pathLength] = 0;

                Open(path);

                free(path);
                break;
            default:
                break;
            }
        }
    }
}

void ShellInstance::Open(char* path){
    
}

void ShellInstance::Update(){
    PollCommands();
}