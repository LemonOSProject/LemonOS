#pragma once

#include <lemon/ipc/endpoint.h>

namespace Lemon::Shell {
    static const char* shellSocketAddress = "lemonshell";
    
    enum ShellWindowState{
        ShellWindowStateNormal,
        ShellWindowStateActive,
        ShellWindowStateMinimized,
    };

    enum {
        LemonShellToggleMenu,
        LemonShellOpen,
        LemonShellAddWindow,
        LemonShellRemoveWindow,
        LemonShellSetWindowState,
    };

    struct ShellCommand{
        short cmd;
        union
        {
            struct {
            int windowID;
            short windowState;
            short titleLength;
            char windowTitle[];
            };
            struct {
                short pathLength;
                char path[];
            };
        };
        
    };

    void AddWindow(int id, short state, const char* title, Endpoint& client);
    void RemoveWindow(int id, Endpoint& client);
    void SetWindowState(int id, int state, Endpoint& client);
    
    void Open(const char* path, Endpoint& client);
    void Open(const char* path);

    void ToggleMenu(Endpoint& client);

    void ToggleMenu();
}