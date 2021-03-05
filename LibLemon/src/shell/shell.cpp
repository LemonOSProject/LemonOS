#include <Lemon/Core/Shell.h>

#include <Lemon/IPC/Interface.h>
#include <string.h>
#include <stdlib.h>

namespace Lemon::Shell {
    void AddWindow(long id, short state, const std::string& title, Endpoint& client){
        client.Queue(Message(Lemon::Shell::LemonShellAddWindow, id, state, title));
    }

    void RemoveWindow(long id, Endpoint& client){
        client.Queue(Message(Lemon::Shell::LemonShellRemoveWindow, id));
    }

    void SetWindowState(long id, short state, Endpoint& client){
        client.Queue(Message(Lemon::Shell::LemonShellSetWindowState, id, state));
    }
    
    void Open(const char* path, Endpoint& client){
        (void)path;
        (void)client;
    }

    void Open(const char* path){
        (void)path;
    }

    void ToggleMenu(Endpoint& client){
        client.Queue(Message(Lemon::Shell::LemonShellToggleMenu));
    }

    void ToggleMenu(){
        return;
    }
}
