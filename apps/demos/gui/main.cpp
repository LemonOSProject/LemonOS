#include <lemon/graphics/Graphics.h>
#include <lemon/graphics/Surface.h>
#include <lemon/gui/Window.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

surface_t fbSurface;

rgba_colour_t backgroundColor = {64, 128, 128, 255};

std::vector<Lemon::GUI::ContextMenuEntry> cxt;

void OnPaint(surface_t* surface){
    Lemon::Graphics::DrawRect(0, 0, 640, 480, 255, 0, 0, surface);
}

int main(){
    Lemon::GUI::Window* win = new Lemon::GUI::Window("Test Window", {640, 480}, WINDOW_FLAGS_RESIZABLE, Lemon::GUI::WindowType::GUI);

    Lemon::GUI::Button* button = new Lemon::GUI::Button("I am a Button", {10, 10, 125, 24});
    win->AddWidget(button);
    button->SetLayout(Lemon::GUI::LayoutSize::Fixed, Lemon::GUI::LayoutSize::Fixed, Lemon::GUI::WidgetAlignment::WAlignRight);

    Lemon::GUI::Button* button2 = new Lemon::GUI::Button("I am a Looooong Button", {10, 10, 145, 24});
    win->AddWidget(button2);
    button2->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Fixed, Lemon::GUI::WidgetAlignment::WAlignLeft);
    
    Lemon::GUI::TextBox* tBox = new Lemon::GUI::TextBox({10, 40, 10, 20}, false);
    win->AddWidget(tBox);
    tBox->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Fixed, Lemon::GUI::WidgetAlignment::WAlignLeft);

    Lemon::GUI::Bitmap* drawBox = new Lemon::GUI::Bitmap({10, 70, 400, 100}); // Rectangle clip test
    win->AddWidget(drawBox);
    rect_t r1 = {10, 20, 300, 70};
    rect_t r2 = {50, 10, 100, 60};
    auto rClips = r1.Split(r2);
    for(auto rect : rClips){
        Lemon::Graphics::DrawRect(rect, (rgba_colour_t){255, 255, 0, 255}, &drawBox->surface);
        Lemon::Graphics::DrawString("r1 clip", rect.x, rect.y, 0, 0, 0, &drawBox->surface);
    }

    Lemon::Graphics::DrawRect(r2, {0, 255, 255, 255}, &drawBox->surface);

    Lemon::GUI::ListView* lView = new Lemon::GUI::ListView({10, 180, 10, 100});
    win->AddWidget(lView);
    lView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Fixed, Lemon::GUI::WidgetAlignment::WAlignLeft);

    Lemon::GUI::GridView* gridView = new Lemon::GUI::GridView({10, 290, 10, 10});
    win->AddWidget(gridView);
    gridView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);

    surface_t folderIcon;
    Lemon::Graphics::LoadImage("/system/lemon/resources/icons/folder.png", &folderIcon);
    surface_t fileIcon;
    Lemon::Graphics::LoadImage("/system/lemon/resources/icons/file.png", &fileIcon);

    Lemon::GUI::GridItem item1;
    item1.name = "Item";
    item1.icon = &folderIcon;

    Lemon::GUI::GridItem item2;
    item2.name = "Another Item";
    item2.icon = &fileIcon;

    for(int i = 0; i < 16; i++){
        gridView->AddItem(item1);
    }

    for(int i = 0; i < 16; i++){
        gridView->AddItem(item2);
    }

    Lemon::GUI::ListColumn col1;
    col1.displayWidth = 220;
    col1.name = "Name";
    col1.editable = true;
    
    Lemon::GUI::ListColumn col2;
    col2.displayWidth = 130;
    col2.name = "Random Integer";
    
    Lemon::GUI::ContextMenuEntry ent;
    ent.id = 1;
    ent.name = std::string("Add Item...");

    cxt.push_back(ent);

    ent.id = 2;
    ent.name = std::string("Exit");

    cxt.push_back(ent);

    while(!win->closed){
		Lemon::WindowServer::Instance()->Poll();

        win->GUIPollEvents();

        win->Paint();
        Lemon::WindowServer::Instance()->Wait();
    }

    delete win;
    return 0;
}