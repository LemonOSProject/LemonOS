#include <Lemon/GUI/Widgets.h>

#include <Lemon/Core/IconManager.h>
#include <Lemon/Core/Keyboard.h>
#include <Lemon/GUI/Messagebox.h>
#include <Lemon/GUI/Theme.h>
#include <Lemon/GUI/Window.h>

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace Lemon::GUI {
const Surface* FileView::diskIcon = nullptr;
const Surface* FileView::folderIcon = nullptr;
const Surface* FileView::fileIcon = nullptr;
const Surface* FileView::textFileIcon = nullptr;
const Surface* FileView::jsonFileIcon = nullptr;
const Surface* FileView::ramIcon = nullptr;

const Surface* FileView::diskIconSml = nullptr;
const Surface* FileView::folderIconSml = nullptr;

void FileViewOnListSelect(GridItem& item, GridView* lv) {
    FileView* fv = (FileView*)lv->GetParent();

    fv->onFileSelected(item.name);
}

class FileButton : public Button {
public:
    std::string file;
    int icon = 0;
    FileButton(const char* _label, rect_t _bounds) : Button(_label, _bounds) { drawText = false; }

    void Paint(surface_t* surface) {
        const Surface* iconS = nullptr;
        switch (icon) {
        case 1:
            iconS = FileView::diskIconSml;
            break;
        case 2:
            iconS = FileView::diskIconSml; // ramIcon;
            break;
        default:
            iconS = FileView::folderIconSml;
            break;
        }

        if (Graphics::PointInRect(fixedBounds, window->lastMousePos)) {
            Graphics::DrawRect(fixedBounds, Theme::Current().ColourForeground(), surface);
        }

        if (iconS && iconS->buffer)
            Graphics::surfacecpyTransparent(surface, iconS, bounds.pos + (vector2i_t){2, 2});

        Graphics::DrawString(label.c_str(), bounds.pos.x + 20, bounds.pos.y + bounds.size.y / 2 - 8,
                             Theme::Current().ColourTextLight(), surface);
    }
};

void OnFileButtonPress(FileButton* b) {
    FileView* fv = ((FileView*)b->GetParent());
    fv->currentPath = "/";
    fv->currentPath.append(b->file);

    fv->Refresh();
}

FileView::FileView(rect_t bounds, const char* path) : Container(bounds) {
    if (!diskIcon) {
        diskIcon = IconManager::Instance()->GetIcon("disk", IconManager::IconSize64x64);
        folderIcon = IconManager::Instance()->GetIcon("folder", IconManager::IconSize64x64);
        fileIcon = IconManager::Instance()->GetIcon("file", IconManager::IconSize64x64);
        textFileIcon = IconManager::Instance()->GetIcon("filetext", IconManager::IconSize64x64);
        jsonFileIcon = IconManager::Instance()->GetIcon("filejson", IconManager::IconSize64x64);
        ramIcon = IconManager::Instance()->GetIcon("ram", IconManager::IconSize64x64);
        diskIconSml = IconManager::Instance()->GetIcon("disk", IconManager::IconSize16x16);
        folderIconSml = IconManager::Instance()->GetIcon("folder", IconManager::IconSize16x16);
    }

    currentPath = path;

    fileList = new GridView({sidepanelWidth, pathBoxHeight + pathBoxPadding.y * 2, 0, 0});
    AddWidget(fileList);
    fileList->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WidgetAlignment::WAlignLeft);

    fileList->OnSubmit = OnListSubmit;
    fileList->OnSelect = FileViewOnListSelect;

    pathBox = new TextBox({pathBoxPadding.x, pathBoxPadding.y, pathBoxPadding.x, pathBoxHeight}, false);
    AddWidget(pathBox);
    pathBox->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WidgetAlignment::WAlignLeft);
    pathBox->OnSubmit = OnTextBoxSubmit;

    DIR* root = opendir("/");
    assert(root);

    int ypos = pathBox->GetFixedBounds().height + 20;
    char str[NAME_MAX];
    while (struct dirent* ent = readdir(root)) {
        int icon = 3;

        if (strcmp(ent->d_name, "lib") == 0 || strcmp(ent->d_name, "etc") == 0) {
            continue;
        }

        if (strncmp(ent->d_name, "hd", 2) == 0 && ent->d_name[2]) { // hd(x)?
            snprintf(str, NAME_MAX, "Harddrive (%s)", ent->d_name);
            icon = 1;
        } else if (strcmp(ent->d_name, "system") == 0) {
            strcpy(str, "System");
            icon = 0;
        } else if (strcmp(ent->d_name, "dev") == 0) { // dev?
            snprintf(str, NAME_MAX, "Devices (%s)", ent->d_name);
            icon = 3;
        } else if (strcmp(ent->d_name, "initrd") == 0) { // initrd?
            snprintf(str, NAME_MAX, "Ramdisk (%s)", ent->d_name);
            icon = 2;
        } else
            sprintf(str, "%s", ent->d_name);
        strcat(ent->d_name, "/");
        FileButton* fb = new FileButton(str, (rect_t){bounds.pos + (vector2i_t){2, ypos}, {sidepanelWidth - 4, 20}});
        fb->file = ent->d_name;
        fb->icon = icon;
        fb->e.onPress.Set(OnFileButtonPress, fb);

        AddWidget(fb);

        ypos += 22;
    }
    closedir(root);

    Refresh();
}

void FileView::Refresh() {
    char* rPath = realpath(currentPath.c_str(), nullptr);
    assert(rPath);

    fileList->selected = -1;

    currentPath = rPath;
    free(rPath);

    if (currentPath.back() != '/')
        currentPath.append("/");

    pathBox->LoadText(currentPath.c_str());

    onPathChanged(currentPath);

    fileList->ClearItems();

    std::string absPath;
    struct dirent** entries;
    int entryCount =
        scandir(currentPath.c_str(), &entries, static_cast<int (*)(const dirent*)>([](const dirent* d) -> int {
                    (void)d;
                    return 1;
                }),
                static_cast<int (*)(const dirent**, const dirent**)>([](const dirent** a, const dirent** b) {
                    if ((*a)->d_type == DT_DIR && (*b)->d_type != DT_DIR) {
                        return -1;
                    } else if ((*b)->d_type == DT_DIR && (*a)->d_type != DT_DIR) {
                        return 1;
                    }
                    return strcmp((*a)->d_name, (*b)->d_name);
                }));

    if (entryCount < 0) {
        perror("GUI: FileView: open:");
        return;
    }

    for (int i = 0; i < entryCount; i++) {
        struct dirent& dirent = *entries[i];

        GridItem item;
        item.name = dirent.d_name;

        absPath = currentPath + "/" + dirent.d_name;

        struct stat statResult;
        int ret = lstat(absPath.c_str(), &statResult);
        if (ret) {
            perror("GUI: FileView: File: Stat");
            // assert(!ret);
            continue;
        }

        if (S_ISDIR(statResult.st_mode)) {
            item.icon = folderIcon;
        } else if (char* ext = strchr(dirent.d_name, '.'); ext) {
            if (!strcmp(ext, ".txt") || !strcmp(ext, ".cfg") || !strcmp(ext, ".py") || !strcmp(ext, ".asm")) {
                item.icon = textFileIcon;
            } else if (!strcmp(ext, ".json")) {
                item.icon = jsonFileIcon;
            } else {
                item.icon = fileIcon;
            }
        } else {
            item.icon = fileIcon;
        }

        fileList->AddItem(item);
    }

    while (--entryCount >= 0) {
        free(entries[entryCount]);
    }
    free(entries);
}

void FileView::OnSubmit(std::string& path) {
    std::string absPath;

    if (path[0] == '/') { // Absolute Path
        absPath = path;
    } else { // Relative Path
        absPath = currentPath + path;
    }

    struct stat statResult;
    int ret = lstat(absPath.c_str(), &statResult);

    if (ret) {
        perror("GUI: FileView: OnSubmit: Stat:");
        char msg[512];
        sprintf(msg, "Error opening file %s (Error Code: %d)", absPath.c_str(), ret);
        DisplayMessageBox("Open...", msg, MsgButtonsOK);
        return;
    }

    if (S_ISDIR(statResult.st_mode)) {
        currentPath = std::move(absPath);

        Refresh();
    } else {
        onFileOpened(absPath);
    }
}

void FileView::OnListSubmit(GridItem& item, GridView* list) {
    FileView* fv = (FileView*)list->GetParent();

    fv->OnSubmit(item.name);
}

void FileView::OnTextBoxSubmit(TextBox* textBox) {
    FileView* fv = (FileView*)textBox->GetParent();

    fv->OnSubmit(textBox->contents[0]);
}
} // namespace Lemon::GUI