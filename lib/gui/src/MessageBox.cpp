#include <lemon/gui/Messagebox.h>

#include <lemon/gui/Widgets.h>
#include <lemon/gui/Window.h>
#include <lemon/graphics/Font.h>

namespace Lemon::GUI {
int pressed = 1;
void OnMessageBoxOKPressed(Lemon::GUI::Window* b) {
    pressed = 1;
    b->closed = true;
}

void OnMessageBoxCancelPressed(Lemon::GUI::Window* b) {
    pressed = 0;
    b->closed = true;
}

int DisplayMessageBox(const char* title, const char* message, MsgBoxButtons buttons) {
    int width = Graphics::GetTextLength(message) + 10;
    if (width < 220)
        width = 220;

    Window* win = new Window(title, {width, 80}, 0, WindowType::GUI);

    Label* label = new Label(message, {10, 10, 180, 12});
    win->AddWidget(label);

    if (buttons == MsgButtonsOKCancel) {
        Button* okBtn = new Button("OK", {-52, 2, 100, 24});
        win->AddWidget(okBtn);
        okBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignCentre, WAlignBottom);
        okBtn->e.onPress.Set(OnMessageBoxOKPressed, win);

        Button* cancelBtn = new Button("Cancel", {52, 2, 100, 24});
        win->AddWidget(cancelBtn);
        cancelBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignCentre, WAlignBottom);
        cancelBtn->e.onPress.Set(OnMessageBoxCancelPressed, win);
    } else {
        Button* okBtn = new Button("OK", {0, 5, 100, 24});
        win->AddWidget(okBtn);
        okBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignCentre, WAlignBottom);
        okBtn->e.onPress.Set(OnMessageBoxOKPressed, win);
    }

    while (!win->closed) {
        Lemon::WindowServer::Instance()->Poll();

        win->GUIPollEvents();
        win->Paint();

        Lemon::WindowServer::Instance()->Wait();
    }

    delete win;

    return pressed;
}
} // namespace Lemon::GUI