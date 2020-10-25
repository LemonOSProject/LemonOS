#include <gui/widgets.h>

#include <core/keyboard.h>
#include <string>
#include <math.h>
#include <gui/colours.h>
#include <gui/messagebox.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>

#ifdef __lemon__
    #include <lemon/filesystem.h>
#endif

namespace Lemon::GUI {
    surface_t FileView::diskIcon;
    surface_t FileView::folderIcon;
    surface_t FileView::fileIcon;
    surface_t FileView::ramIcon;

    surface_t FileView::diskIconSml;
    surface_t FileView::folderIconSml;

    __attribute__((constructor))
    void InitializeFVIcons(){
        if(FILE* f = fopen("/initrd/disk.png", "rb")){
            Graphics::LoadImage(f, &FileView::diskIcon);
            fclose(f);
        } else {
            printf("GUI: Warning: Could not load FileView icons!");
            FileView::diskIcon.buffer = nullptr;
            FileView::diskIcon.width = 0;
        }

        if(FILE* f = fopen("/initrd/folder.png", "rb")){
            Graphics::LoadImage(f, &FileView::folderIcon);
            fclose(f);
        } else {
            printf("GUI: Warning: Could not load FileView icons!");
            FileView::folderIcon.buffer = nullptr;
            FileView::folderIcon.width = 0;
        }

        if(FILE* f = fopen("/initrd/file.png", "rb")){
            Graphics::LoadImage(f, &FileView::fileIcon);
            fclose(f);
        } else {
            printf("GUI: Warning: Could not load FileView icons!");
            FileView::fileIcon.buffer = nullptr;
            FileView::fileIcon.width = 0;
        }

        if(FILE* f = fopen("/initrd/disksml.png", "rb")){
            Graphics::LoadImage(f, &FileView::diskIconSml);
            fclose(f);
        } else {
            printf("GUI: Warning: Could not load FileView icons!");
            FileView::diskIconSml.buffer = nullptr;
            FileView::diskIconSml.width = 0;
        }

        if(FILE* f = fopen("/initrd/foldersml.png", "rb")){
            Graphics::LoadImage(f, &FileView::folderIconSml);
            fclose(f);
        } else {
            printf("GUI: Warning: Could not load FileView icons!");
            FileView::folderIconSml.buffer = nullptr;
            FileView::folderIconSml.width = 0;
        }
    }

    void FileViewOnListSelect(GridItem& item, GridView* lv){
        FileView* fv = (FileView*)lv->GetParent();

        if(fv->OnFileSelected)
            fv->OnFileSelected(item.name, fv);
    }
    
	class FileButton : public Button{
    public:
        std::string file;
        int icon = 0;
        FileButton(const char* _label, rect_t _bounds) :  Button(_label, _bounds){
            drawText =  false;
        }

		void Paint(surface_t* surface){
            surface_t* iconS = nullptr;
            switch(icon){
                case 1:
                    iconS = &FileView::diskIconSml;
                    break;
                case 2:
                    iconS = &FileView::diskIconSml;//ramIcon;
                    break;
                default:
                    iconS = &FileView::folderIconSml;
                    break;
            }

            if(iconS && iconS->buffer)
                Graphics::surfacecpyTransparent(surface, iconS, bounds.pos + (vector2i_t){2, 2});

            Graphics::DrawString(label.c_str(), bounds.pos.x + 20, bounds.pos.y + bounds.size.y / 2 - 8, colours[Colour::Text], surface);
        }
	};

    void OnFileButtonPress(Button* b){
        FileView* fv = ((FileView*)b->GetParent());
        fv->currentPath = "/";
        fv->currentPath.append(((FileButton*)b)->file);

        fv->Refresh();
    }
    
    FileView::FileView(rect_t bounds, const char* path, void(*_OnFileOpened)(const char*, FileView*)) : Container(bounds) {
        OnFileOpened = _OnFileOpened;
        currentPath = path;

        fileList = new GridView({sidepanelWidth, 24, 0, 0});
        AddWidget(fileList);
        fileList->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WidgetAlignment::WAlignLeft);

        fileList->OnSubmit = OnListSubmit;
        fileList->OnSelect = FileViewOnListSelect;

        pathBox = new TextBox({2, 2, 2, 20}, false);
        AddWidget(pathBox);
        pathBox->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WidgetAlignment::WAlignLeft);
        pathBox->OnSubmit =  OnTextBoxSubmit;

        int sideBar = open("/", O_DIRECTORY);
        int ypos = pathBox->GetFixedBounds().height + 20;
        char str[270];
        int i = 0;
        lemon_dirent_t dirent;
        while(lemon_readdir(sideBar, i++, &dirent) > 0){
            int icon = 3;

            printf("Name: %s\n", dirent.name);

            if(strcmp(dirent.name, "lib") == 0 || strcmp(dirent.name, "etc") == 0){
                continue;
            }

            if(strncmp(dirent.name, "hd", 2) == 0 && dirent.name[2]){ // hd(x)?
                sprintf(str, "Harddrive (%s)", dirent.name);
                icon = 1;
            } else if(strcmp(dirent.name, "system") == 0){
                strcpy(str, "System");
                icon = 0;
            } else if(strcmp(dirent.name, "dev") == 0){ // dev?
                sprintf(str, "Devices (%s)", dirent.name);
                icon = 3;
            } else if(strcmp(dirent.name, "initrd") == 0){ // initrd?
                sprintf(str, "Ramdisk (%s)", dirent.name);
                icon = 2;
            } else sprintf(str, dirent.name);
            strcat(dirent.name, "/");
            FileButton* fb = new FileButton(str, (rect_t){bounds.pos + (vector2i_t){2, ypos}, {sidepanelWidth - 4,20}});
            fb->file = dirent.name;
            fb->icon = icon;
            fb->OnPress = OnFileButtonPress;

            AddWidget(fb);

            ypos += 22;
        }
        close(sideBar);

        Refresh();
    }

    void FileView::Refresh(){
        char* rPath = realpath(currentPath.c_str(), nullptr);
        assert(rPath);

        currentPath = rPath;
        free(rPath);

        #ifdef __lemon__

        close(currentDir);

        if(currentPath.back() != '/')
            currentPath.append("/");

        currentDir = open(currentPath.c_str(), O_DIRECTORY);

        if(currentDir <= 0){
            perror("GUI: FileView: open:");
            return;
        }

        pathBox->LoadText(currentPath.c_str());

        fileList->ClearItems();

        std::string absPath;

        int i = 0;
        lemon_dirent_t dirent;
        while(lemon_readdir(currentDir, i++, &dirent) > 0){
            GridItem item;
            item.name = dirent.name;

            absPath = currentPath + "/" + dirent.name;

            struct stat statResult;
            int ret = stat(absPath.c_str(), &statResult);
            if(ret){
                perror("GUI: FileView: File: Stat:");
                assert(!ret);
                return;
            }

            if(S_ISDIR(statResult.st_mode)){
                item.icon = &folderIcon;
            } else {
                item.icon = &fileIcon;
            }

            fileList->AddItem(item);
        }

        #endif
    }

    void FileView::OnSubmit(std::string& path){
        std::string absPath;

        if(path[0] == '/'){ // Absolute Path
            absPath = path;
        } else { // Relative Path
            absPath = currentPath + path;
        }

        struct stat statResult;
        int ret = stat(absPath.c_str(), &statResult);

        if(ret){
            perror("GUI: FileView: OnSubmit: Stat:");
            char msg[512];
            sprintf(msg, "Error opening file %s (Error Code: %d)", absPath.c_str(), ret);
            DisplayMessageBox("Open...", msg, MsgButtonsOK);
            return;
        }

        if(S_ISDIR(statResult.st_mode)){
            currentPath = absPath;

            Refresh();
        } else if(OnFileOpened) {
            OnFileOpened(absPath.c_str(), this);
        }
    }
    
    void FileView::OnListSubmit(GridItem& item, GridView* list){
        FileView* fv = (FileView*)list->GetParent();

        fv->OnSubmit(item.name);
    }

    void FileView::OnTextBoxSubmit(TextBox* textBox){
        FileView* fv = (FileView*)textBox->GetParent();

        fv->OnSubmit(textBox->contents[0]);
    }
}