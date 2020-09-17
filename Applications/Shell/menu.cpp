#include <lemon/fb.h>
#include <lemon/syscall.h>

#include <gfx/graphics.h>

#include <gui/window.h>

#include <core/msghandler.h>
#include <core/cfgparser.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <stdexcept>

Lemon::GUI::Window* window;
extern fb_info_t videoInfo;

const char* itemsPath = "/system/lemon/menu";

struct MenuObject{
    std::string name;
};

struct MenuCategory : MenuObject{
    std::vector<Lemon::GUI::ContextMenuEntry> items;
};

struct MenuItem : MenuObject{
    int id;
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
};

std::map<int, MenuItem> items;

std::map<std::string, MenuCategory> categories;
std::vector<MenuItem> rootItems;

Lemon::GUI::LayoutContainer* menuContainer;

class MenuWidget : public Lemon::GUI::Widget{
    MenuObject obj;
    Lemon::Graphics::TextObject label = Lemon::Graphics::TextObject();

public:
    MenuWidget(MenuObject& obj, rect_t bounds) : Widget(bounds){
        this->obj = obj;

        label = Lemon::Graphics::TextObject(fixedBounds.pos + (vector2i_t){0, fixedBounds.size.y / 2}, obj.name);
    }

    void Paint(surface_t* surface){
        if(Lemon::Graphics::PointInRect(fixedBounds, window->lastMousePos)){
            Lemon::Graphics::DrawRect(fixedBounds, Lemon::colours[Lemon::Colour::Foreground], surface);
        }

        label.Render(surface);
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
    window = new Lemon::GUI::Window("", {240, 300}, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL, Lemon::GUI::WindowType::GUI, {0, videoInfo.height - 30 - 300});

    categories["Games"] = MenuCategory();
    categories["Utilities"] = MenuCategory();
    categories["Other"] = MenuCategory();

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

    int itemsDir = open("/system/lemon/menu/items", O_DIRECTORY);
    if(itemsDir >= 0){
        struct dirent** entries = nullptr;
        int entryCount = scandir("/system/lemon/menu/items", &entries, static_cast<int(*)(const dirent*)>([](const dirent* d) -> int { return strcmp(d->d_name, "..") && strcmp(d->d_name, "."); }), static_cast<int(*)(const dirent**, const dirent**)>([](const dirent** a, const dirent**b) { return strcmp((*a)->d_name, (*b)->d_name); }));

        for(int i = 0; i < entryCount; i++){
            char* entryPath = new char[strlen("/system/lemon/menu/items") + strlen(entries[i]->d_name) + 1];
            strcpy(entryPath, "/system/lemon/menu/items");
            strcat(entryPath, entries[i]->d_name);

            CFGParser itemCParser = CFGParser(entryPath);
            itemCParser.Parse();
            for(auto& heading : itemCParser.GetItems()){
                if(!heading.first.compare("Item")){
                    MenuItem item;
                    std::string categoryName;
                    MenuCategory cat;

                    for(auto& cItem : heading.second){
                        if(!cItem.name.compare("name")){
                            item.name = cItem.value;
                        } else if(!cItem.name.compare("comment")){
                            item.comment = cItem.value;
                        } else if(!cItem.name.compare("exec")){
                            item.exec = cItem.value;
                        } else if(!cItem.name.compare("category")){
                            item.name = cItem.value;
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
                        cat = categories[categoryName];
                    } catch(std::out_of_range e){
                        cat = categories["Other"];
                    }

                    items[item.id] = item;
                    cat.items.push_back({.id = item.id, .name = item.name});
                }
            }
        }

        if(entries){
            free(entries);
        }
    }
    close(itemsDir);

    menuContainer = new Lemon::GUI::LayoutContainer({0, 0, 0, 0}, {216, 36});

    menuContainer->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);
    window->AddWidget(menuContainer);

    for(auto& cat : categories){
        menuContainer->AddWidget(new MenuWidget(cat.second, {0, 0, 0, 0})); // LayoutContainer will handle bounds
    }

    for(auto& item : rootItems){
        menuContainer->AddWidget(new MenuWidget(item, {0, 0, 0, 0})); // LayoutContainer will handle bounds
    }

    window->Paint();
}

Lemon::MessageHandler& GetWindowHandler(){
    return window->GetHandler();
}