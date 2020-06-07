#pragma once

namespace Lemon::GUI {
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
        LemonShellSetActive,
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
}