#include <lemon/fb.h>
#include <lemon/syscall.h>
#include <lemon/spawn.h>

#include <gfx/graphics.h>

#include <gui/window.h>

#include <core/msghandler.h>
#include <core/cfgparser.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include <stdexcept>

Lemon::GUI::Window* window;
extern fb_info_t videoInfo;

extern bool showMenu;
const char* itemsPath = "/system/lemon/menu/";

std::vector<std::string> path = {"/system/bin/"};

class MenuObject{
public:
    std::string name;

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
        window->DisplayContextMenu(items, pos);
    }
};

class MenuItem : public MenuObject{
public:
    uint16_t id;
    std::string comment;
    std::string exec;

    MenuItem(){

    }

    MenuItem(std::string name, std::string comment, std::string exec, int id){
        this->name = name;
        this->comment = comment;
        this->exec = exec;
        this->id = id;
    }

    void Open(vector2i_t pos) final {
        for(auto& p : path){
            char* execPath = new char[p.length() + exec.length()];
            strcpy(execPath, p.c_str());
            strcat(execPath, exec.c_str());

            struct stat st;
            int ret = stat(execPath, &st);
            if(!ret){
                lemon_spawn(execPath, 1, &execPath);

                showMenu = false;
                window->Minimize();
            }

            delete execPath;
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

        label = Lemon::Graphics::TextObject(fixedBounds.pos + (vector2i_t){0, fixedBounds.size.y / 2}, this->obj->name);
    }

    void Paint(surface_t* surface){
        if(Lemon::Graphics::PointInRect(fixedBounds, window->lastMousePos)){
            Lemon::Graphics::DrawRect(fixedBounds, Lemon::colours[Lemon::Colour::Foreground], surface);
        }

        label.Render(surface);
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
	Lemon::Graphics::DrawGradientVertical(0, 0, 24, surface->height, {42, 50, 64}, {96, 96, 96}, surface);
}

int nextID = 10000;
int GetItemID(){
    return nextID++;
}

void InitializeMenu(){
	syscall(SYS_GET_VIDEO_MODE, (uintptr_t)&videoInfo,0,0,0,0);
    window = new Lemon::GUI::Window("", {240, 300}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::GUI, {0, static_cast<int>(videoInfo.height) - 30 - 300});
    window->OnPaint = OnPaint;
    window->rootContainer.background = {0, 0, 0, 0};

    categories["Games"] = MenuCategory("Games");
    categories["Utilities"] = MenuCategory("Utilities");
    categories["Other"] = MenuCategory("Other");

    {
        MenuItem item("Terminal...", "Open a Terminal", "terminal.lef", GetItemID());
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
                            item.exec = cItem.value;
                        } else if(!cItem.name.compare("categories")){
                            categoryName = cItem.value;
                        }
                    }

                    if(!item.name.length() || !item.exec.length()){
                        continue; // Zero length name or path
                    }
                    item.id = GetItemID();

                    if(!categoryName.length()){
                        categoryName = "Other";
                    }
                    try{
                        cat = &categories[categoryName];
                    } catch(std::out_of_range e){
                        cat = &categories["Other"];
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

    menuContainer = new Lemon::GUI::LayoutContainer({24, 0, 0, 0}, {216, 36});

    menuContainer->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);
    window->AddWidget(menuContainer);

    menuContainer->AddWidget(new MenuWidget(categories["Utilities"], {0, 0, 0, 0}));
    menuContainer->AddWidget(new MenuWidget(categories["Games"], {0, 0, 0, 0}));
    menuContainer->AddWidget(new MenuWidget(categories["Other"], {0, 0, 0, 0}));

    for(auto& item : rootItems){
        menuContainer->AddWidget(new MenuWidget(item, {0, 0, 0, 0})); // LayoutContainer will handle bounds
    }

    window->Paint();
}

static bool paint = false;
void PollMenu(){
    Lemon::LemonEvent ev;
    while(window->PollEvent(ev)){
        if(ev.event == Lemon::EventWindowCommand){
            try{
                auto& item = items[ev.windowCmd];
                item.Open({0, 0});
            } catch(std::out_of_range e){

            }
        } else {
            window->GUIHandleEvent(ev);
        }
        paint = true;
    }

    if(paint){
        window->Paint();
        paint = false;
    }
}

Lemon::MessageHandler& GetMenuWindowHandler(){
    return window->GetHandler();
}

void MinimizeMenu(bool s){
    window->Minimize(s);
}