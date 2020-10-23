#include <stdio.h>
#include <assert.h>

#include <gfx/graphics.h>
#include <gfx/surface.h>

#include <gui/window.h>
#include <stdlib.h>

#include <core/msghandler.h>

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

    Lemon::GUI::ListView* lView = new Lemon::GUI::ListView({10, 180, 10, 10});
    win->AddWidget(lView);
    lView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::WidgetAlignment::WAlignLeft);

    Lemon::GUI::ListColumn col1;
    col1.displayWidth = 220;
    col1.name = "Name";
    
    Lemon::GUI::ListColumn col2;
    col2.displayWidth = 130;
    col2.name = "Random Integer";

    lView->AddColumn(col1);
    lView->AddColumn(col2);

    Lemon::GUI::ContextMenuEntry ent;
    ent.id = 1;
    ent.name = std::string("Add Item...");

    cxt.push_back(ent);

    ent.id = 2;
    ent.name = std::string("Exit");

    cxt.push_back(ent);

    int i = 0;
    for(; i < 10; i++){
        Lemon::GUI::ListItem item;
        int randI = rand() % 1000;
        char buf[80];
        sprintf(buf, "Item #%d", i);
        item.details.push_back(buf);
        item.details.push_back(std::to_string(randI));
        lView->AddItem(item);
    }

    while(!win->closed){
        Lemon::LemonEvent ev;
        while(win->PollEvent(ev)){
            win->GUIHandleEvent(ev);
        }

        win->Paint();

        win->WaitEvent();
    }

    delete win;
    return 0;
}