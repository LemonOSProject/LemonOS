#include <stdio.h>
#include <assert.h>

#include <gfx/graphics.h>
#include <gfx/surface.h>

#include <gui/window.h>
#include <stdlib.h>

#include <core/msghandler.h>

surface_t fbSurface;

rgba_colour_t backgroundColor = {64, 128, 128};

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

    Lemon::GUI::ListView* lView = new Lemon::GUI::ListView({10, 80, 10, 10});
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

    for(int i = 0; i < 10; i++){
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
    }

    delete win;
    return 0;
}