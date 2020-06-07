#include <gui/widgets.h>

#include <core/keyboard.h>
#include <string>
#include <math.h>
#include <gui/colours.h>
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
    surface_t FileView::icons = { .width = 0, .buffer = nullptr };

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
            
            Graphics::DrawString(label, bounds.pos.x + 20, bounds.pos.y + bounds.size.y / 2 - 8, 0, 0, 0, surface);
        }
	};

    void OnFileButtonPress(Button* b){
        FileView* fv = ((FileView*)b->GetParent());
        fv->currentPath = "/";
        fv->currentPath.append(((FileButton*)b)->file);

        fv->Refresh();
    }
    
    FileView::FileView(rect_t bounds, char* path, void(*_OnFileOpened)(char*, FileView*)) : Container(bounds) {
        OnFileOpened = _OnFileOpened;
        currentPath = path;

        fileList = new ListView({sidepanelWidth, 24, 0, 0});
        AddWidget(fileList);
        fileList->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch, WidgetAlignment::WAlignLeft);

        fileList->OnSubmit = OnListSubmit;

        nameCol.name = "Name";
        nameCol.displayWidth = 300;
        sizeCol.name = "Size";
        sizeCol.displayWidth = 50;

        fileList->AddColumn(nameCol);
        fileList->AddColumn(sizeCol);

        pathBox = new TextBox({2, 2, 2, 20}, false);
        AddWidget(pathBox);
        pathBox->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WidgetAlignment::WAlignLeft);
        pathBox->OnSubmit =  OnTextBoxSubmit;

        int sideBar = open("/", O_RDONLY);
        int ypos = pathBox->GetFixedBounds().height + 20;
        char str[150];
        int i = 0;
        lemon_dirent_t dirent;
        while(lemon_readdir(sideBar, i++, &dirent)){
            int icon = 3;
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

        pathBox->LoadText(currentPath.c_str());

        fileList->ClearItems();

        #ifdef __lemon__

        if(currentPath.back() != '/')
            currentPath.append("/");

        currentDir = open(currentPath.c_str(), O_DIRECTORY);

        if(currentDir <= 0){
            perror("GUI: FileView: open:");
            assert(currentDir > 0);
            return;
        }

        char buf[PATH_MAX];

        int i = 0;
        lemon_dirent_t dirent;
        while(lemon_readdir(currentDir, i++, &dirent)){
            ListItem item;
            item.details.push_back(dirent.name);

            if(dirent.type & FS_NODE_DIRECTORY) {
                fileList->AddItem(item);
                continue;
            }

            sprintf(buf, "%s%s", currentPath.c_str(), dirent.name);
            
            int fileFd = open(buf, O_RDONLY);

            if(fileFd <= 0){
                perror("GUI: FileView: File: open:");
                assert(fileFd > 0);
                return;
            }

            struct stat statResult;
            int ret = fstat(fileFd, &statResult);

            close(fileFd);

            if(ret){
                perror("GUI: FileView: File: Stat:");
                assert(!ret);
                return;
            }

            char buf[80];
            sprintf(buf, "%d KB", statResult.st_size / 1024);

            item.details.push_back(buf);

            fileList->AddItem(item);
        }

        #endif
    }

    void FileView::OnSubmit(ListItem& item, ListView* list){
        char buf[PATH_MAX];

        sprintf(buf, "%s%s", currentPath.c_str(), item.details[0].c_str());
        
        int fileFd = open(buf, O_RDONLY);

        if(fileFd <= 0){
            perror("GUI: FileView: OnSubmit: File: open:");
            assert(fileFd > 0);
            return;
        }

        struct stat statResult;
        int ret = fstat(fileFd, &statResult);

        close(fileFd);

        if(ret){
            perror("GUI: FileView: OnSubmit: File: Stat:");
            assert(!ret);
            return;
        }

        if(S_ISDIR(statResult.st_mode)){
            currentPath = buf;

            Refresh();
        } else if(OnFileOpened) {
            OnFileOpened(buf, this);
        }
    }
    
    void FileView::OnTextSubmit(){
        int fileFd = open(pathBox->contents[0].c_str(), O_RDONLY);

        if(fileFd <= 0){
            printf("Warning: GUI: FileView: Invalid Path\n");
            return; // Invalid Path
        }

        struct stat statResult;
        int ret = fstat(fileFd, &statResult);

        close(fileFd);

        if(ret){
            perror("GUI: FileView: OnTextSubmit: File: Stat:");
            assert(!ret);
            return;
        }

        if(S_ISDIR(statResult.st_mode)){
            currentPath =  pathBox->contents[0].c_str();

            Refresh();
        }
    }

    void FileView::OnListSubmit(ListItem& item, ListView* list){
        FileView* fv = (FileView*)list->GetParent();

        fv->OnSubmit(item, list);
    }

    void FileView::OnTextBoxSubmit(TextBox* textBox){
        FileView* fv = (FileView*)textBox->GetParent();

        fv->OnTextSubmit();
    }
}