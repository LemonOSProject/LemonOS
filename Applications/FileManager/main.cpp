#include <Lemon/Core/Shell.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Widgets.h>
#include <Lemon/GUI/Messagebox.h>
#include <Lemon/System/Spawn.h>
#include <Lemon/System/Util.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

void OnFileOpened(const char* path, Lemon::GUI::FileView* fv){
	Lemon::Shell::Open(path);
}

extern "C"
int main(int argc, char** argv){
	Lemon::GUI::Window* window;

	window = new Lemon::GUI::Window("File Manager", {600, 348}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);
	
	Lemon::GUI::FileView* fv = new Lemon::GUI::FileView({{0,0},{0,0}}, "/", OnFileOpened);
	window->AddWidget(fv);
	fv->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);

	while(!window->closed){
		Lemon::WindowServer::Instance()->Poll();

		window->GUIPollEvents();

		window->Paint();
        Lemon::WindowServer::Instance()->Wait();
	}

	delete window;
	return 0;
}
