#include <gfx/window/widgets.h>

#include <lemon/filesystem.h>
#include <gfx/window/messagebox.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#define HARD_DISK_ICON 0
#define RAM_DISK_ICON 1
#define DEV_FS_ICON 1

const int sideBarWidth = 124;

namespace Lemon::GUI{
    static surface_t icons = { .buffer = nullptr};
    
	class FileButton : public Button{
    public:
        char* file;
        FileButton(const char* _label, rect_t _bounds) : Button(_label, _bounds) { drawText = false; }
        int icon = 0;
        FileView* fv;
		void Paint(surface_t* surface){
            Button::Paint(surface);
            Graphics::surfacecpy(surface, &icons, bounds.pos + (vector2i_t){2, 2}, (rect_t){{icon * 16, 0}, {16, 16}});
            Graphics::DrawString(label, bounds.pos.x + 20, bounds.pos.y + bounds.size.y / 2 - 8, 0, 0, 0, surface);
        }
        ~FileButton(){
            free(file);
        }
	};

    void OnFileButtonPress(Button* b){
        FileView* fv = ((FileButton*)b)->fv;
        strcpy(fv->currentPath, "/");
        strcat(fv->currentPath, ((FileButton*)b)->file);

        fv->Reload();
    }

    //////////////////////////
    // FileView
    //////////////////////////

    FileView::FileView(rect_t _bounds, char* path, char** _filePointer, void(*_fileOpened)(char*, char**)) : ListView(_bounds){
        if(!icons.buffer) {
            if(FILE* f = fopen("/initrd/icons.png", "rb")){
                Graphics::LoadImage(f, &icons);
                fclose(f);
            }
        }
        
        bounds = _bounds;
        filePointer = _filePointer;
        OnFileOpened = _fileOpened;

        iBounds.pos.y += pathBoxHeight;
        iBounds.size.y -= pathBoxHeight;

        iBounds.pos.x += sideBarWidth;
        iBounds.size.x -= sideBarWidth;
        
        currentPath = (char*)malloc(PATH_MAX);
        strcpy(currentPath, path);

        currentDir = open(currentPath, O_RDONLY);
        int i = 0;
        lemon_dirent_t dirent;
        while(lemon_readdir(currentDir, i++, &dirent)){
            contents.add_back(new ListItem(dirent.name));
        }

        int sideBar = open("/", O_RDONLY);
        i = 0;
        int ypos = pathBoxHeight + 2;
        char str[150];
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
            FileButton* fb = new FileButton(str, {bounds.pos + (vector2i_t){2, ypos}, {sideBarWidth - 4,24}});
            fb->file = strdup(dirent.name);
            fb->icon = icon;
            fb->fv = this;
            fb->OnPress = OnFileButtonPress;

            children.add_back(fb);

            ypos += 26;
        }
        close(sideBar);

        ResetScrollBar();
    }

    void FileView::Refresh(){
        close(currentDir);

        currentDir = lemon_open(currentPath, 666);

        if(!currentDir){
            MessageBox("Failed to open directory!", MESSAGEBOX_OK);
        }
        
        for(int i = contents.get_length() - 1; i >= 0; i--){
            delete contents.get_at(i);
        }
        contents.clear();
        
        int i = 0;
        lemon_dirent_t dirent;
        while(lemon_readdir(currentDir, i++, &dirent)){
            contents.add_back(new ListItem(dirent.name));
        }
        
        ResetScrollBar();
    }

    void FileView::Paint(surface_t* surface){
        Graphics::DrawRect(bounds.pos.x + 2, bounds.pos.y + 2, bounds.size.x - 4, 16, 255, 255, 255, surface);
        Graphics::DrawString(currentPath, bounds.pos.x + 4, bounds.pos.y + 4, 0, 0, 0, surface);

        ListView::Paint(surface);

        for(int i = 0; i < children.get_length(); i++){
            children[i]->Paint(surface);
        }
    }

    void FileView::OnMouseDown(vector2i_t mousePos){
        if(mousePos.y - bounds.pos.y > pathBoxHeight){
            if(mousePos.x > sideBarWidth){
                ListView::OnMouseDown(mousePos);
            } else {
                for(int i = 0; i < children.get_length(); i++){
                    if(Graphics::PointInRect(children[i]->bounds, mousePos)){
                        children[i]->OnMouseDown(mousePos);
                    }
                }
            }
        }
    }

    void FileView::OnMouseUp(vector2i_t mousePos){
        if(mousePos.y - bounds.pos.y > pathBoxHeight){
            if(mousePos.x > sideBarWidth){
                ListView::OnMouseUp(mousePos);
            } else {
                for(int i = 0; i < children.get_length(); i++){
                    if(Graphics::PointInRect(children[i]->bounds, mousePos)){
                        children[i]->OnMouseUp(mousePos);
                    }
                }
            }
        }
    }

    void FileView::OnSubmit(){
        ListItem* item = contents.get_at(selected);
        lemon_dirent_t dirent;
        lemon_readdir(currentDir, selected, &dirent);
        if(dirent.type & FS_NODE_DIRECTORY){
            strcpy(currentPath + strlen(currentPath), dirent.name);
            strcpy(currentPath + strlen(currentPath), "/");

            Reload();
        } else if(OnFileOpened) {
            char* str = (char*)malloc(strlen(currentPath) + strlen(dirent.name) + 1 /*Separator*/ + 1 /*Null Terminator*/);

            strcpy(str, currentPath);
            strcpy(str + strlen(str), "/");
            strcpy(str + strlen(str), dirent.name);

            OnFileOpened(str, filePointer);

            free(str);
        }
    }

    void FileView::Reload(){
        close(currentDir);
        currentDir = lemon_open(currentPath, 666);

        lemon_dirent_t dirent;

        if(!currentDir){
            MessageBox("Failed to open directory!", MESSAGEBOX_OK);
        }
        
        for(int i = contents.get_length() - 1; i >= 0; i--){
            delete contents.get_at(i);
        }
        contents.clear();
        
        int i = 0;
        while(lemon_readdir(currentDir, i++, &dirent)){
            contents.add_back(new ListItem(dirent.name));
        }

        ResetScrollBar();
    }

    FileView::~FileView(){
        children.~List();
    }
}