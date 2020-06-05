#include <gui/shell.h>
#include <gui/window.h>

#include <map>

struct ShellWindow{
	int id;
	char* title;
	int state;
};

class ShellInstance {
    Lemon::MessageServer shellSrv;

    Lemon::GUI::Window* taskbar;
    Lemon::GUI::Window* menu;

    void PollCommands();
public:
    std::map<int, ShellWindow*> windows;
    ShellWindow* active = nullptr;
    bool showMenu = true;

    ShellInstance(Lemon::GUI::Window* taskbar, Lemon::GUI::Window* menu);

    void Update();
    void Open(char* path);
};