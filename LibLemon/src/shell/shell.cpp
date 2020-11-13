#include <core/shell.h>

#include <core/msghandler.h>
#include <string.h>
#include <stdlib.h>

namespace Lemon::Shell {

    void AddWindow(int id, short state, const char* title, MessageClient& client){
        Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::Shell::ShellCommand) + strlen(title));
        msg->length = sizeof(ShellCommand) + strlen(title);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_SHELLCMD;

        auto shCmd = (Lemon::Shell::ShellCommand*)msg->data;
        shCmd->cmd = Lemon::Shell::LemonShellAddWindow;
        shCmd->windowID = id;
        shCmd->titleLength = strlen(title);
        shCmd->windowState = state;

        strncpy(shCmd->windowTitle, title, shCmd->titleLength);

        client.Send(msg); // Tell the Shell to add the window to its list
    }

    void RemoveWindow(int id, MessageClient& client){
        Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::Shell::ShellCommand));
        msg->length = sizeof(ShellCommand);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_SHELLCMD;

        auto shCmd = (Lemon::Shell::ShellCommand*)msg->data;
        shCmd->cmd = Lemon::Shell::LemonShellRemoveWindow;
        shCmd->windowID = id;

        client.Send(msg);
    }

    void SetWindowState(int id, int state, MessageClient& client){
        Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::Shell::ShellCommand));
        msg->length = sizeof(ShellCommand);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_SHELLCMD;

        auto shCmd = (Lemon::Shell::ShellCommand*)msg->data;
        shCmd->cmd = Lemon::Shell::LemonShellSetWindowState;
        shCmd->windowID = id;
        shCmd->windowState = state;

        client.Send(msg);
    }
    
    void Open(const char* path, MessageClient& client){
        (void)path;
        (void)client;
    }

    void Open(const char* path){
        (void)path;
    }

    void ToggleMenu(MessageClient& client){
        Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::Shell::ShellCommand));
        msg->length = sizeof(ShellCommand);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_SHELLCMD;

        auto shCmd = (Lemon::Shell::ShellCommand*)msg->data;
        shCmd->cmd = Lemon::Shell::LemonShellToggleMenu;

        client.Send(msg);
    }

    void ToggleMenu(){
        MessageClient client;
        
        sockaddr_un shellAddr;
        strcpy(shellAddr.sun_path, Lemon::Shell::shellSocketAddress);
        shellAddr.sun_family = AF_UNIX;
        client.Connect(shellAddr, sizeof(sockaddr_un));

        ToggleMenu(client);
    }
}
