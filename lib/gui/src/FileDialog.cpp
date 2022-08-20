#include <lemon/gui/Window.h>

#include <lemon/gui/FileDialog.h>
#include <lemon/gui/Messagebox.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Lemon::GUI {
__thread char* selectedPth = nullptr; // TODO: Better solution
__thread FileView* dialogFileView = nullptr;
__thread TextBox* dialogFileBox = nullptr;
__thread int dflags = 0;

void FileDialogOnFileOpened(FileView*, std::string path) {
    if (!(dflags & FILE_DIALOG_CREATE) || access(path.c_str(), W_OK) ||
        DisplayMessageBox(
            "Open...", "File already exists! Overwrite?",
            MsgButtonsOKCancel)) { // Only open if create flag not specified OR user responds ok to message
        if (selectedPth) {
            free(selectedPth);
        }

        selectedPth = realpath(path.c_str(), NULL);
    }
}

void FileDialogOnFileSelected(void*, std::string path) {
    dialogFileBox->contents.front() = std::move(path);
}

void FileDialogOnPathChanged(void*, std::string) {
    dialogFileBox->contents.front().clear();
}

void FileDialogOnCancelPress(Lemon::GUI::Window* window) {
    window->closed = true; // Tell FileDialog() that we want to close the window
}

void FileDialogOnFileBoxSubmit(Lemon::GUI::TextBox* box) {
    if (box->contents.size() > NAME_MAX) {
        DisplayMessageBox("Open...", "Filename is invalid!", MsgButtonsOK);
        return;
    }

    // Check for a leading slash
    std::string path;
    if(box->contents.front().front() != '/') {
        path = dialogFileView->currentPath;
    }
    path += box->contents.front();

    struct stat sResult;

    int e = stat(path.c_str(), &sResult);
    if (e && dflags & FILE_DIALOG_CREATE && errno == ENOENT) {
        FileDialogOnFileOpened(dialogFileView, path);
        return;
    } else if (e && errno == ENOENT) {
        char buf[512];
        sprintf(buf, "File %s not found!", path.c_str());
        DisplayMessageBox("Open...", buf, MsgButtonsOK);
        return;
    } else if (e) {
        perror("GUI: FileDialog: ");
        char buf[512];
        sprintf(buf, "Error opening file %s (Error code: %d)", path.c_str(), errno);
        DisplayMessageBox("Open...", buf, MsgButtonsOK);
        return;
    } else if ((sResult.st_mode & S_IFMT) == S_IFDIR && !(dflags & FILE_DIALOG_DIRECTORIES)) {
        dialogFileView->OnSubmit(path);
        return;
    } else {
        FileDialogOnFileOpened(dialogFileView, path);
    }
}

void FileDialogOnOKPress(void*) { FileDialogOnFileBoxSubmit(dialogFileBox); }

char* FileDialog(const char* path, int flags) {
    dflags = flags;

    if (!path) {
        path = "."; // Current Directory
    }

    selectedPth = nullptr;
    dialogFileView = nullptr;
    dialogFileBox = nullptr;

    Window* win = new Window("Open...", {540, 420}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);

    FileView* fv = new FileView({0, 0, 0, 62}, path);
    win->AddWidget(fv);
    fv->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WAlignLeft);
    fv->onFileSelected.Set(FileDialogOnFileSelected);
    fv->onFileOpened.Set(FileDialogOnFileOpened);
    fv->onPathChanged.Set(FileDialogOnPathChanged);

    dialogFileView = fv;

    Button* okBtn = new Button("OK", {5, 34, 100, 24});
    win->AddWidget(okBtn);
    okBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignRight, WAlignBottom);
    okBtn->e.onPress.Set(FileDialogOnOKPress);

    Button* cancelBtn = new Button("Cancel", {5, 5, 100, 24});
    win->AddWidget(cancelBtn);
    cancelBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignRight, WAlignBottom);
    cancelBtn->e.onPress.Set(FileDialogOnCancelPress, win);

    TextBox* fileBox = new TextBox({fv->SidepanelWidth(), 34, 110, 24}, false);
    win->AddWidget(fileBox);
    fileBox->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WAlignLeft, WAlignBottom);
    fileBox->OnSubmit = FileDialogOnFileBoxSubmit;
    dialogFileBox = fileBox;

    win->Paint();

    while (!win->closed && !selectedPth) {
        Lemon::WindowServer::Instance()->Poll();

        win->GUIPollEvents();
        win->Paint();
        Lemon::WindowServer::Instance()->Wait();
    }

    delete win;

    return selectedPth;
}
} // namespace Lemon::GUI