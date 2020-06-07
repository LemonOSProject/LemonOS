#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/window.h>
#include <gui/widgets.h>
#include <stdlib.h>
#include <list.h>
#include <lemon/filesystem.h>
#include <fcntl.h>
#include <unistd.h>
#include <gui/messagebox.h>
#include <lemon/spawn.h>
#include <lemon/util.h>

void OnFileOpened(char* path, Lemon::GUI::FileView* fv){
	if(strncmp(path + strlen(path) - 4, ".lef", 4) == 0){
		lemon_spawn(path, 1, &path);
	} else if(strncmp(path + strlen(path) - 4, ".txt", 4) == 0 || strncmp(path + strlen(path) - 4, ".cfg", 4) == 0){
		char* argv[] = {"/initrd/textedit.lef", path};
		lemon_spawn("/initrd/textedit.lef", 2, argv);
	} else if(strncmp(path + strlen(path) - 4, ".png", 4) == 0 || strncmp(path + strlen(path) - 4, ".bmp", 4) == 0){
		char* argv[] = {"/initrd/imgview.lef", path};
		lemon_spawn("/initrd/imgview.lef", 2, argv);
	}
}

extern "C"
int main(int argc, char** argv){
	Lemon::GUI::Window* window;

	window = new Lemon::GUI::Window("Fileman", {512, 256}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
	
	Lemon::GUI::FileView* fv = new Lemon::GUI::FileView({{0,0},{0,0}}, "/", OnFileOpened);
	window->AddWidget(fv);
	fv->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);
	
	bool repaint = true;

	for(;;){
		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			window->GUIHandleEvent(ev);
		}

		if(repaint)
			window->Paint();

		lemon_yield();
	}

	for(;;);
}