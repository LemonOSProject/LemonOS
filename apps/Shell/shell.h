#include <Lemon/Core/Shell.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/IPC/Interface.h>
#include <Lemon/Services/lemon.shell.h>

#include <map>

class ShellWindow {
public:
    long id;
    std::string title;
    int state;
    int lastState = Lemon::WindowState_Normal;

    ShellWindow(long id, std::string title, int state) : id(id), title(std::move(title)), state(state) {
        
    }
};

class ShellInstance : Shell {
    Lemon::Interface shellSrv;

    Lemon::GUI::Window* taskbar;
    Lemon::GUI::Window* menu;

public:
    ShellInstance(const Lemon::Handle& svc, const char* ifName);

    void OnPeerDisconnect(const Lemon::Handle& client) override;
    void OnOpen(const Lemon::Handle& client, const std::string& url) override;
    void OnToggleMenu(const Lemon::Handle& client) override;

    void SetMenu(Lemon::GUI::Window* menu);
    void SetTaskbar(Lemon::GUI::Window* taskbar);

    void Poll();

    void SetWindowState(ShellWindow* win);

    inline Lemon::Interface& GetInterface() { return shellSrv; }
};

extern bool showMenu;

Lemon::GUI::Window* InitializeMenu();
void PollMenu();
void MinimizeMenu(bool s);