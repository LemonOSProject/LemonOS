#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Widgets.h>
#include <Lemon/GUI/Messagebox.h>
#include <Lemon/System/Spawn.h>
#include <Lemon/System/Util.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

void OnFileOpened(const char* path, Lemon::GUI::FileView* fv){
	char* pathCopy = strdup(path);

	if(strncmp(path + strlen(path) - 4, ".lef", 4) == 0){
		lemon_spawn(path, 1, &pathCopy);
	} else if(strncmp(path + strlen(path) - 4, ".txt", 4) == 0 || strncmp(path + strlen(path) - 4, ".cfg", 4) == 0 || strncmp(path + strlen(path) - 4, ".asm", 4) == 0 || strncmp(path + strlen(path) - 3, ".py", 3) == 0 || strncmp(path + strlen(path) - 5, ".json", 5) == 0){
		char* const argv[] = {const_cast<char*>("/system/bin/textedit.lef"), pathCopy};
		lemon_spawn("/system/bin/textedit.lef", 2, argv);
	} else if(strncmp(path + strlen(path) - 4, ".png", 4) == 0 || strncmp(path + strlen(path) - 4, ".bmp", 4) == 0){
		char* const argv[] = {const_cast<char*>("/system/bin/imgview.lef"), pathCopy};
		lemon_spawn("/system/bin/imgview.lef", 2, argv);
	}

	free(pathCopy);
}

extern "C"
int main(int argc, char** argv){
	Lemon::GUI::Window* window;

	window = new Lemon::GUI::Window("Fileman", {600, 348}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
	
	Lemon::GUI::FileView* fv = new Lemon::GUI::FileView({{0,0},{0,0}}, "/", OnFileOpened);
	window->AddWidget(fv);
	fv->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);

	while(!window->closed){
		Lemon::WindowServer::Instance()->Poll();

		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			window->GUIHandleEvent(ev);
		}

		window->Paint();
        Lemon::WindowServer::Instance()->Wait();
	}

	delete window;
	return 0;
}