#include <Lemon/System/Framebuffer.h>
#include <Lemon/System/Spawn.h>

#include <lemon/syscall.h>

#include <Lemon/Graphics/Graphics.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/Core/CFGParser.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include <stdexcept>

Lemon::GUI::Window* menuWindow;
extern fb_info_t videoInfo;

extern bool showMenu;
const char* itemsPath = "/system/lemon/menu/";

std::vector<std::string> path = {"/system/bin/"};
surface_t defaultIcon = {.width = 0, .height = 0, .depth = 32, .buffer = nullptr};

class MenuObject{
public:
    std::string name;
    surface_t icon = defaultIcon;

    virtual void Open(vector2i_t pos) = 0;

    virtual ~MenuObject() = default;
};

class MenuCategory : public MenuObject{
public:
    std::vector<Lemon::GUI::ContextMenuEntry> items;

    MenuCategory(){

    }

    MenuCategory(std::string name){
        this->name = name;
    }

    void Open(vector2i_t pos) final {
        menuWindow->DisplayContextMenu(items, pos);
    }
};

class MenuItem : public MenuObject{
    std::string exec;
public:
    uint16_t id;
    std::string comment;
    std::vector<char*> args;

    MenuItem(){
        icon = defaultIcon;
    }

    MenuItem(const std::string& name, const std::string& comment, const std::string& exec, int id){
        this->name = name;
        this->comment = comment;
        this->exec = exec;
        this->id = id;
        icon = defaultIcon;

        std::string temp;
        for(char c : exec){
            if(c == ' '){
                args.push_back(strdup(temp.c_str()));

                temp.clear();
            } else {
                temp += c;
            }
        }

        if(temp.length()){
            args.push_back(strdup(temp.c_str())); // Any args left over?
        }
    }

    void SetExec(const std::string& e){
        exec = e;

        args.clear();

        std::string temp;
        for(char c : exec){
            if(c == ' '){
                args.push_back(strdup(temp.c_str()));

                temp.clear();
            } else {
                temp += c;
            }
        }

        if(temp.length()){
            args.push_back(strdup(temp.c_str())); // Any args left over?
        }
    }

    std::string& GetExec(){
        return exec;
    }

    void Open(vector2i_t pos) final {
        for(auto& p : path){
            char* execPath = new char[p.length() + strlen(args[0])];
            strcpy(execPath, p.c_str());
            strcat(execPath, args[0]);

            struct stat st;
            int ret = stat(execPath, &st);
            if(!ret){
                lemon_spawn(execPath, args.size(), args.data());

                showMenu = false;
                menuWindow->Minimize();
            }

            delete[] execPath;
        }
    }
};

std::map<uint16_t, MenuItem> items;

std::map<std::string, MenuCategory> categories;
std::vector<MenuItem> rootItems;

Lemon::GUI::LayoutContainer* menuContainer;

class MenuWidget : public Lemon::GUI::Widget{
    MenuObject* obj;
    Lemon::Graphics::TextObject label = Lemon::Graphics::TextObject();

public:
    MenuWidget(MenuObject& obj, rect_t bounds) : Widget(bounds){
        this->obj = &obj;
    }

    void Paint(surface_t* surface){
        if(Lemon::Graphics::PointInRect(fixedBounds, menuWindow->lastMousePos)){
            Lemon::Graphics::DrawRect(fixedBounds, Lemon::colours[Lemon::Colour::Foreground], surface);
        }

        if(obj->icon.width){
            Lemon::Graphics::surfacecpyTransparent(surface, &obj->icon, fixedBounds.pos + (vector2i_t){fixedBounds.size.x / 2 - obj->icon.width / 2, fixedBounds.size.y / 2 - obj->icon.height / 2});
        }
    }

    void OnMouseEnter(vector2i_t pos){
        window->tooltipOwner = this;
        window->SetTooltip(obj->name.c_str(), window->GetPosition() + (vector2i_t){ window->GetSize().x + 8, fixedBounds.y + (fixedBounds.height / 2 - 8)});
    }

    void OnMouseExit(vector2i_t pos){
        if(window->tooltipOwner == this){
            window->HideTooltip();
        }
    }

    void OnMouseUp(vector2i_t pos){
        obj->Open(fixedBounds.pos + (vector2i_t){fixedBounds.size.x, 0});
    }

    void UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        label.SetPos(fixedBounds.pos + (vector2i_t){2, fixedBounds.size.y / 2});
    }
};

void OnPaint(surface_t* surface){

}

int nextID = 10000;
int GetItemID(){
    return nextID++;
}

void InitializeMenu(){
	syscall(SYS_GET_VIDEO_MODE, (uintptr_t)&videoInfo,0,0,0,0);
    menuWindow = new Lemon::GUI::Window("", {36, static_cast<int>(videoInfo.height) - 36}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::GUI, {0, 0});
    menuWindow->OnPaint = OnPaint;
    menuWindow->rootContainer.background = {0, 0, 0, 0};

    Lemon::Graphics::LoadImage("/initrd/applicationicon.png", &defaultIcon);

    categories["Games"] = MenuCategory("Games");
    Lemon::Graphics::LoadImage("/initrd/gamesicon.png", &categories["Games"].icon);

    categories["Utilities"] = MenuCategory("Utilities");
    categories["Other"] = MenuCategory("Other");

    {
        MenuItem item("Terminal...", "Open a Terminal", "terminal.lef", GetItemID());
        Lemon::Graphics::LoadImage("/initrd/terminalicon.png", &item.icon);
        items[item.id] = item;
        rootItems.push_back(item);
    }

    {
        MenuItem item("Run...", "Run", "run.lef", GetItemID());
        items[item.id] = item;
        rootItems.push_back(item);
    }

    int itemsDir = open(itemsPath, O_DIRECTORY);
    if(itemsDir >= 0){
        struct dirent** entries = nullptr;
        int entryCount = scandir(itemsPath, &entries, static_cast<int(*)(const dirent*)>([](const dirent* d) -> int { return strcmp(d->d_name, "..") && strcmp(d->d_name, "."); }), static_cast<int(*)(const dirent**, const dirent**)>([](const dirent** a, const dirent**b) { return strcmp((*a)->d_name, (*b)->d_name); }));

        for(int i = 0; i < entryCount; i++){
            char* entryPath = new char[strlen(itemsPath) + strlen(entries[i]->d_name) + 1];
            strcpy(entryPath, itemsPath);
            strcat(entryPath, entries[i]->d_name);

            CFGParser itemCParser = CFGParser(entryPath);
            itemCParser.Parse();
            for(auto& heading : itemCParser.GetItems()){
                if(!heading.first.compare("Item")){
                    MenuItem item;
                    std::string categoryName;
                    MenuCategory* cat;

                    for(auto& cItem : heading.second){
                        if(!cItem.name.compare("name")){
                            item.name = cItem.value;
                        } else if(!cItem.name.compare("comment")){
                            item.comment = cItem.value;
                        } else if(!cItem.name.compare("exec")){
                            item.SetExec(cItem.value);
                        } else if(!cItem.name.compare("categories")){
                            categoryName = cItem.value;
                        }
                    }

                    if(!item.name.length() || !item.GetExec().length()){
                        continue; // Zero length name or path
                    }
                    item.id = GetItemID();

                    if(!categoryName.length()){
                        categoryName = "Other";
                    }
                    try{
                        cat = &categories.at(categoryName);
                    } catch(const std::out_of_range& e){
                        cat = &categories.at("Other");
                    }

                    items[item.id] = item;
                    cat->items.push_back({.id = item.id, .name = item.name});
                }
            }
        }

        if(entries){
            free(entries);
        }
    }
    close(itemsDir);

    menuContainer = new Lemon::GUI::LayoutContainer({0, 0, 0, 0}, {36, 36});
    menuContainer->background = {0x1d, 0x1c, 0x1b, 255};
    menuContainer->xPadding = 0;
    menuContainer->yPadding = 0;

    menuContainer->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);
    menuWindow->AddWidget(menuContainer);

    for(auto& cat : categories){
        menuContainer->AddWidget(new MenuWidget(cat.second, {0, 0, 0, 0}));
    }

    for(auto& item : rootItems){
        menuContainer->AddWidget(new MenuWidget(item, {0, 0, 0, 0})); // LayoutContainer will handle bounds
    }

    menuWindow->Paint();
}

static bool paint = false;
void PollMenu(){
    Lemon::LemonEvent ev;
    while(menuWindow->PollEvent(ev)){
        if(ev.event == Lemon::EventWindowCommand){
            try{
                auto& item = items.at(ev.windowCmd);
                item.Open({0, 0});
            } catch(const std::out_of_range& e){

            }
        } else {
            menuWindow->GUIHandleEvent(ev);
        }
        paint = true;
    }

    if(paint){
        menuWindow->Paint();
        paint = false;
    }
}

void MinimizeMenu(bool s){
    menuWindow->Minimize(s);
}