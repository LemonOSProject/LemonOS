#include <lemon/gui/window.h>

#include <sys/stat.h>
#include <lemon/gui/filedialog.h>
#include <lemon/gui/messagebox.h>
#include <unistd.h>
#include <errno.h>

namespace Lemon::GUI{
	__thread char* selectedPth = nullptr; // TODO: Better solution
	__thread FileView* dialogFileView = nullptr;
	__thread TextBox* dialogFileBox = nullptr;
	__thread int dflags = 0;

	void FileDialogOnFileOpened(const char* path, __attribute__((unused)) FileView* fv){
		if(!(dflags & FILE_DIALOG_CREATE) || access(path, W_OK) || DisplayMessageBox("Open...", "File already exists! Overwrite?", MsgButtonsOKCancel)) { // Only open if create flag not specified OR user responds ok to message
			if(selectedPth){
				free(selectedPth);
			}

			selectedPth = strdup(path);
		}
	}

	void FileDialogOnFileSelected(std::string& path, __attribute__((unused)) FileView* fv){
		dialogFileBox->contents.front() = path;
	}

	void FileDialogOnCancelPress(Lemon::GUI::Button* btn){
		btn->GetWindow()->closed = true; // Tell FileDialog() that we want to close the window
	}

	void FileDialogOnFileBoxSubmit(Lemon::GUI::TextBox* box){
		if(box->contents.front().find('/') != std::string::npos && box->contents.size() > NAME_MAX){
			DisplayMessageBox("Open...", "Filename is invalid!", MsgButtonsOK);
			return;
		}

		std::string path = dialogFileView->currentPath;
		path += box->contents.front();

		struct stat sResult;

		int e = stat(path.c_str(), &sResult);
		if(e && dflags & FILE_DIALOG_CREATE && errno == ENOENT){
			FileDialogOnFileOpened(path.c_str(), dialogFileView);
			return;
		} else if(e && errno == ENOENT) {
			char buf[512];
			sprintf(buf, "File %s not found!", path.c_str());
			DisplayMessageBox("Open...", buf, MsgButtonsOK);
			return;
		} else if(e){
			perror("GUI: FileDialog: ");
			char buf[512];
			sprintf(buf, "Error opening file %s (Error code: %d)", path.c_str(), errno);
			DisplayMessageBox("Open...", buf, MsgButtonsOK);
			return;
		} else if((sResult.st_mode & S_IFMT) == S_IFDIR && !(dflags & FILE_DIALOG_DIRECTORIES)) {
			dialogFileView->OnSubmit(path);
			return;
		} else {
			FileDialogOnFileOpened(path.c_str(), dialogFileView);
		}
	}

	void FileDialogOnOKPress(__attribute__((unused)) Lemon::GUI::Button* btn){
		FileDialogOnFileBoxSubmit(dialogFileBox);
	}

	char* FileDialog(const char* path, int flags){
		dflags = flags;

		if(!path){
			path = "."; // Current Directory
		}

		selectedPth = nullptr;
		dialogFileView = nullptr;
		dialogFileBox = nullptr;

		Window* win = new Window("Open...", {504, 300}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);

		FileView* fv = new FileView({0, 0, 0, 63}, path, FileDialogOnFileOpened);
		win->AddWidget(fv);
		fv->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WAlignLeft);
		fv->OnFileSelected = FileDialogOnFileSelected;

		dialogFileView = fv;

		Button* okBtn = new Button("OK", {5, 34, 100, 24});
		win->AddWidget(okBtn);
		okBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignRight, WAlignBottom);
		okBtn->OnPress = FileDialogOnOKPress;

		Button* cancelBtn = new Button("Cancel", {5, 5, 100, 24});
		win->AddWidget(cancelBtn);
		cancelBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignRight, WAlignBottom);
		cancelBtn->OnPress = FileDialogOnCancelPress;

		TextBox* fileBox = new TextBox({125, 36, 110, 20}, false);
		win->AddWidget(fileBox);
		fileBox->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WAlignLeft, WAlignBottom);
		fileBox->OnSubmit = FileDialogOnFileBoxSubmit;
		dialogFileBox = fileBox;

		win->Paint();

		while(!win->closed && !selectedPth){
			LemonEvent ev;
			while(win->PollEvent(ev)){
				win->GUIHandleEvent(ev);
			}

			win->Paint();

			win->WaitEvent();
		}

		delete win;

		return selectedPth;
	}
}