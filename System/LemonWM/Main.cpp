#include "WM.h"

#include <Lemon/Core/Framebuffer.h>
#include <Lemon/Core/Logger.h>
#include <Lemon/System/Spawn.h>

int main() {
    Surface displaySurface;
    Lemon::CreateFramebufferSurface(displaySurface);

    Lemon::Logger::Debug("Initializing LemonWM...");

    WM wm(displaySurface);

    lemon_spawn("/system/bin/terminal.lef", 1, (char * const[]){"terminal.lef"});

    wm.Run();
    return 0;
}
