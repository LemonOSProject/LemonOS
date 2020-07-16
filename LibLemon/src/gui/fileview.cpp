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
    surface_t FileView::icons;

    __attribute__((constructor))
    void InitializeFVIcons(){
        if(FILE* f = fopen("/initrd/icons.png", "rb")){
            Graphics::LoadImage(f, &FileView::icons);
            fclose(f);
        } else {
            printf("GUI: Warning: Could not load FileView icons!");
            FileView::icons.buffer = nullptr;
            FileView::icons.width = 0;
        }
    }

    void FileViewOnListSelect(ListItem& item, ListView* lv){
        FileView* fv = (FileView*)lv->GetParent();

        if(fv->OnFileSelected)
            fv->OnFileSelected(item.details[0], fv);
    }
    
	class FileButton : public Button{
    public:
        std::string file;
        int icon = 0;
        FileButton(const char* _label, rect_t _bounds) :  Button(_label, _bounds){
            drawText =  false;
        }

		void Paint(surface_t* surface){
            Button::Paint(surface);

            if(FileView::icons.buffer)
                Graphics::surfacecpy(surface, &FileView::icons, bounds.pos + (vector2i_t){2, 2}, (rect_t){{icon * 16, 0}, {16, 16}});
            
            Graphics::DrawString(label.c_str(), bounds.pos.x + 20, bounds.pos.y + bounds.size.y / 2 - 8, 0, 0, 0, surface);
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

        fileList = new ListView({sidepanelWidth, 24, 0, 0});
        AddWidget(fileList);
        fileList->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WidgetAlignment::WAlignLeft);

        fileList->OnSubmit = OnListSubmit;
        fileList->OnSelect = FileViewOnListSelect;

        nameCol.name = "Name";
        nameCol.displayWidth = 280;
        sizeCol.name = "Size";
        sizeCol.displayWidth = 80;

        fileList->AddColumn(nameCol);
        fileList->AddColumn(sizeCol);

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

            if(strncmp(dirent.name, "hd", 2) == 0 && dirent.name[2]){ // hd(x)?
                sprintf(str, "Harddrive (%s)", dirent.name);
                icon = 0;
            } else if(strcmp(dirent.name, "dev") == 0){ // dev?
                sprintf(str, "Devices (%s)", dirent.name);
                icon = 2;
            } else if(strcmp(dirent.name, "initrd") == 0){ // initrd?
                sprintf(str, "Ramdisk (%s)", dirent.name);
                icon = 1;
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
            ListItem item;
            item.details.push_back(dirent.name);

            absPath = currentPath + "/" + dirent.name;

            struct stat statResult;
            int ret = stat(absPath.c_str(), &statResult);
            if(ret){
                perror("GUI: FileView: File: Stat:");
                assert(!ret);
                return;
            }

            if(S_ISDIR(statResult.st_mode)){
                fileList->AddItem(item);
                continue;
            }

            char buf[80];
            sprintf(buf, "%lu KB", statResult.st_size / 1024);

            item.details.push_back(std::string(buf));

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
    
    void FileView::OnListSubmit(ListItem& item, ListView* list){
        FileView* fv = (FileView*)list->GetParent();

        fv->OnSubmit(item.details[0]);
    }

    void FileView::OnTextBoxSubmit(TextBox* textBox){
        FileView* fv = (FileView*)textBox->GetParent();

        fv->OnSubmit(textBox->contents[0]);
    }
}