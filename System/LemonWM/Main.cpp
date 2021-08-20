#include "WM.h"

#include <Lemon/Core/ConfigManager.h>
#include <Lemon/Core/Framebuffer.h>
#include <Lemon/Core/Logger.h>
#include <Lemon/System/Spawn.h>

int main() {
    Surface displaySurface;
    Lemon::CreateFramebufferSurface(displaySurface);

    Lemon::Logger::Debug("Initializing LemonWM...");

    ConfigManager config;
    config.AddConfigProperty<std::string>("backgroundImage", "/system/lemon/resources/backgrounds/bg7.png");
    config.AddConfigProperty<std::string>("theme", "/system/lemon/themes/default.json");
    config.AddConfigProperty<bool>("displayFramerate", false);

    config.LoadJSONConfig("/system/lemon/lemonwm.json");

    WM wm(displaySurface);

    wm.Compositor().SetWallpaper(config.GetConfigProperty<std::string>("backgroundImage"));
    wm.Compositor().SetShouldDisplayFramerate(config.GetConfigProperty<bool>("displayFramerate"));

    wm.Run();
    return 0;
}
