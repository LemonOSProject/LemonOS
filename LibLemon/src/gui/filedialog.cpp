#include <gui/window.h>

namespace Lemon::GUI{
	char* selectedPth = nullptr; // TODO: Better solution
	FileView* dialogFileView = nullptr;
	TextBox* dialogFileBox = nullptr;

	void FileDialogOnFileOpened(const char* path, FileView* fv){
		if(selectedPth){
			free(selectedPth);
		}

		selectedPth = strdup(path);
	}

	void FileDialogOnFileSelected(std::string& path, FileView* fv){

	}

	void FileDialogOnCancelPress(Lemon::GUI::Button* btn){
		btn->window->closed = true; // Tell FileDialog() that we want to close the window
	}

	void FileDialogOnOKPress(Lemon::GUI::Button* btn){
		dialogFileView->OnSubmit(dialogFileBox->contents.front());
	}

	void FileDialogOnFileBoxSubmit(Lemon::GUI::TextBox* box){
		dialogFileView->OnSubmit(box->contents.front());
	}

	char* FileDialog(const char* path){
		if(!path){
			path = "."; // Current Directory
		}

		selectedPth = nullptr;
		dialogFileView = nullptr;
		dialogFileBox = nullptr;

		Window* win = new Window("Open...", {480, 300}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);

		FileView* fv = new FileView({0, 0, 0, 63}, path, FileDialogOnFileOpened);
		win->AddWidget(fv);
		fv->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WAlignLeft);

		dialogFileView = fv;

		Button* okBtn = new Button("OK", {5, 34, 100, 24});
		win->AddWidget(okBtn);
		okBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignRight, WAlignBottom);
		okBtn->OnPress = FileDialogOnOKPress;

		Button* cancelBtn = new Button("Cancel", {5, 5, 100, 24});
		win->AddWidget(cancelBtn);
		cancelBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignRight, WAlignBottom);
		cancelBtn->OnPress = FileDialogOnCancelPress;

		TextBox* fileBox = new TextBox({105, 36, 110, 20}, false);
		win->AddWidget(fileBox);
		fileBox->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WAlignLeft, WAlignBottom);
		fileBox->OnSubmit = FileDialogOnFileBoxSubmit;
		dialogFileBox = fileBox;

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