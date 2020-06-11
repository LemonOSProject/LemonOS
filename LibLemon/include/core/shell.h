#pragma once

#include <core/msghandler.h>

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

    void AddWindow(int id, short state, const char* title, MessageClient& client);
    void RemoveWindow(int id, MessageClient& client);
    void SetWindowState(int id, int state, MessageClient& client);
    
    void Open(const char* path, MessageClient& client);
    void Open(const char* path);

    void ToggleMenu(MessageClient& client);

    void ToggleMenu();
}