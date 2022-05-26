#include "shell.h"

#include <Lemon/Core/CFGParser.h>
#include <Lemon/Core/IconManager.h>
#include <Lemon/Core/Keyboard.h>
#include <Lemon/GUI/Model.h>
#include <Lemon/GUI/Theme.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/System/Spawn.h>

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <stdexcept>

#define MENU_WIDTH 500
#define MENU_HEIGHT 274

surface_t defaultIcon = {.width = 0, .height = 0, .depth = 32, .buffer = nullptr};

Lemon::GUI::Window* menuWindow;
Lemon::GUI::ListView* listView;
Lemon::GUI::TextBox* filterBox;

std::list<class MenuItem> menuItems;
std::list<std::string> path;

inline bool StringContainsCaseInsenstive(const std::string& s1, const std::string& s2) {
    return std::search(s1.begin(), s1.end(), s2.begin(), s2.end(),
                       [](char a, char b) { return tolower(a) == tolower(b); }) != s1.end();
}

class MenuItem final {
public:
    std::string name;
    std::string category = "";

    const Surface* icon = &defaultIcon;

    ~MenuItem() = default;

    void Open() {
        // MinimizeMenu(true);
        if (!m_args.size()) {
            return;
        }

        for (auto& p : path) {
            std::string execPath = p + "/" + m_args[0];

            struct stat st;
            int ret = stat(execPath.c_str(), &st);
            if (!ret) {
                lemon_spawn(execPath.c_str(), m_args.size(), m_args.data());
            }
        }
    };

    void SetExec(const std::string& e) {
        m_exec = e;

        m_args.clear();

        std::string temp;
        for (char c : m_exec) {
            if (c == ' ') {
                m_args.push_back(strdup(temp.c_str()));

                temp.clear();
            } else {
                temp += c;
            }
        }

        if (temp.length()) {
            m_args.push_back(strdup(temp.c_str())); // Any args left over?
        }
    }

private:
    std::string m_exec = "";
    std::vector<char*> m_args;
};

class MenuFilter {
public:
    unsigned itemCount;

    MenuFilter() : m_filter(" ") {}

    int ItemCount() const { return m_filteredItems.size(); }

    MenuItem& GetItem(int index) { return *m_filteredItems.at(index); }

    void SetFilter(const std::string& f) {
        if (!m_filter.compare(f)) {
            return; // Filter has not changed
        }

        m_filteredItems.clear();
        m_filter = f;

        if (!m_filter.length()) {
            for (auto& item : menuItems) {
                m_filteredItems.push_back(&item);
            }
            return;
        }

        for (auto& item : menuItems) {
            if (StringContainsCaseInsenstive(item.name, m_filter)) {
                m_filteredItems.push_back(&item);
            } else if (StringContainsCaseInsenstive(item.category, m_filter)) {
                m_filteredItems.push_back(&item);
            }
        }
    }

    void OnSubmit(int selected) {
        if (selected < static_cast<int>(m_filteredItems.size())) {
            m_filteredItems.at(selected)->Open();
        }
    }

private:
    std::vector<MenuItem*> m_filteredItems;
    std::string m_filter;
};

class MenuModel : public Lemon::GUI::DataModel {
public:
    enum {
        ColumnIcon = 0,
        ColumnItemName = 1,
        ColumnCategory = 2,
    };

    MenuFilter filter;

    int ColumnCount() const override { return 3; }
    int RowCount() const override { return filter.ItemCount(); }
    const char* ColumnName(int) const override { return m_colName.c_str(); } // We dont display columns

    Lemon::GUI::Variant GetData(int row, int column) override {
        switch (column) {
        case ColumnIcon:
            return filter.GetItem(row).icon;
        case ColumnItemName:
            return filter.GetItem(row).name;
        case ColumnCategory:
            return filter.GetItem(row).category;
        default:
            return 0;
        }
    }

    int SizeHint(int column) override {
        switch (column) {
        case ColumnIcon:
            return 20;
        case ColumnItemName:
            return MENU_WIDTH - 20 - 80;
        case ColumnCategory:
            return 80;
        default:
            return 0;
        }
    }

private:
    std::string m_colName = "";
} menuModel;

void OnMenuPaint(Surface* surface) {
    Colour bgColor = Lemon::GUI::Theme::Current().ColourContainerBackground();
    int cornerRadius = Lemon::GUI::Theme::Current().WindowCornerRaidus();

    Lemon::Graphics::DrawRoundedRect({0, 0, MENU_WIDTH, MENU_HEIGHT}, bgColor, cornerRadius, cornerRadius, cornerRadius,
                                     cornerRadius, surface);
}

Lemon::GUI::Window* InitializeMenu() {
    std::string pathEnv = getenv("PATH");
    std::string temp;
    for (char c : pathEnv) {
        if (c == ':' && temp.length()) {
            path.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    if (temp.length()) {
        path.push_back(temp);
        temp.clear();
    }

    menuWindow = new Lemon::GUI::Window("", {MENU_WIDTH, MENU_HEIGHT},
                                        WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL | WINDOW_FLAGS_ALWAYS_ACTIVE |
                                            WINDOW_FLAGS_TRANSPARENT,
                                        Lemon::GUI::GUI);
    menuWindow->rootContainer.background.a = 0;
    menuWindow->OnPaint = OnMenuPaint;

    vector2i_t screenBounds = Lemon::WindowServer::Instance()->GetScreenBounds();
    menuWindow->Relocate({screenBounds.x / 2 - 200, screenBounds.y / 2 - 150});

    listView = new Lemon::GUI::ListView({5, 5, 5, 24 + 10});
    listView->SetModel(&menuModel);
    listView->displayColumnNames = false;
    listView->drawBackground = false;

    filterBox = new Lemon::GUI::TextBox({5, 5, 5, 24}, false);

    menuWindow->AddWidget(listView);
    menuWindow->AddWidget(filterBox);
    menuWindow->SetActive(filterBox);

    listView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch,
                        Lemon::GUI::WidgetAlignment::WAlignLeft, Lemon::GUI::WidgetAlignment::WAlignBottom);
    filterBox->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Fixed,
                         Lemon::GUI::WidgetAlignment::WAlignLeft, Lemon::GUI::WidgetAlignment::WAlignTop);

    const char* itemsPath = "/system/lemon/menu/";
    int itemsDir = open(itemsPath, O_DIRECTORY);
    if (itemsDir >= 0) {
        struct dirent** entries = nullptr;
        int entryCount =
            scandir(itemsPath, &entries, static_cast<int (*)(const dirent*)>([](const dirent* d) -> int {
                        return strcmp(d->d_name, "..") && strcmp(d->d_name, ".");
                    }),
                    static_cast<int (*)(const dirent**, const dirent**)>(
                        [](const dirent** a, const dirent** b) { return strcmp((*a)->d_name, (*b)->d_name); }));

        for (int i = 0; i < entryCount; i++) {
            char* entryPath = new char[strlen(itemsPath) + strlen(entries[i]->d_name) + 1];
            strcpy(entryPath, itemsPath);
            strcat(entryPath, entries[i]->d_name);

            CFGParser itemCParser = CFGParser(entryPath);
            itemCParser.Parse();
            for (auto& heading : itemCParser.GetItems()) {
                if (!heading.first.compare("Item")) {
                    MenuItem item;

                    for (auto& cItem : heading.second) {
                        if (!cItem.name.compare("name")) {
                            item.name = cItem.value;
                        } else if (!cItem.name.compare("categories")) {
                            item.category = cItem.value;
                        } else if (!cItem.name.compare("icon")) {
                            item.icon =
                                Lemon::IconManager::Instance()->GetIcon(cItem.value, Lemon::IconManager::IconSize16x16);
                        } else if (!cItem.name.compare("exec")) {
                            item.SetExec(cItem.value);
                        }
                    }

                    if (!item.name.length()) {
                        continue; // Zero length name
                    }

                    menuItems.push_back(item);
                }
            }
        }

        if (entries) {
            free(entries);
        }
    }
    close(itemsDir);

    menuModel.filter.SetFilter("");

    menuWindow->GUIRegisterEventHandler(Lemon::EventKeyPressed, [](Lemon::LemonEvent& ev) -> bool {
        if (ev.key == KEY_ARROW_DOWN || ev.key == KEY_ARROW_UP) { // Send straight to listview
            listView->OnKeyPress(ev.key);
        } else if (ev.key == KEY_ENTER) {
            menuModel.filter.OnSubmit(listView->selected);
        } else {
            filterBox->OnKeyPress(ev.key); // Send straight to textbox
            menuModel.filter.SetFilter(filterBox->contents.front());
        }
        return true;
    });

    menuWindow->GUIRegisterEventHandler(Lemon::EventWindowMinimized, [](Lemon::LemonEvent& ev) -> bool {
        showMenu = false;
        return false;
    });

    menuWindow->GUIRegisterEventHandler(Lemon::EventMouseReleased, [](Lemon::LemonEvent& ev) -> bool {
        if (ev.mousePos.y > 24) {
            menuModel.filter.OnSubmit(listView->selected);
        }

        return true;
    });

    menuWindow->Paint();
    return menuWindow;
}

void PollMenu() {
    menuWindow->GUIPollEvents();
    menuWindow->Paint();
}

void MinimizeMenu(bool minimized) {
    menuWindow->Minimize(minimized);

    showMenu = !minimized;
}
