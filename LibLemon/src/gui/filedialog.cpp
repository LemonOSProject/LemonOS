#include <gui/window.h>

namespace Lemon::GUI{
	char* selectedPth = nullptr; // TODO: Better solution

	void FileDialogOnFileOpened(const char* path, FileView* fv){
		selectedPth = strdup(path);
	}

	char* FileDialog(const char* path){
		if(!path){
			path = "."; // Current Directory
		}

		selectedPth = nullptr;

		Window* win = new Window("Open...", {480, 300}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);

		FileView* fv = new FileView({0, 0, 0, 0}, path, FileDialogOnFileOpened);
		win->AddWidget(fv);
		fv->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WAlignLeft);

		bool paint = true;

		while(!win->closed && !selectedPth){
			LemonEvent ev;
			while(win->PollEvent(ev)){
				win->GUIHandleEvent(ev);
				paint = true;
			}

			if(paint){
				win->Paint();
				paint = false;
			}
		}

		delete win;

		return selectedPth;
	}
}