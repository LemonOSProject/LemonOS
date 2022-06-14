#include <Lemon/Core/Shell.h>

#include <Lemon/GUI/Messagebox.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Widgets.h>

#include <Lemon/Graphics/Graphics.h>

using namespace Lemon;

void OnAboutLemon(void*) {
    Lemon::Shell::Open("/system/bin/sysinfo.lef");
}

void OnThirdParty(void*) {
    Lemon::Shell::Open("/system/lemon/docs/thirdparty.txt");
}

int main(int, char**) {
    vector2i_t screenBounds = Lemon::WindowServer::Instance()->GetScreenBounds();
    GUI::Window* window = new GUI::Window("Welcome to Lemon OS!", {400, 400}, 0, GUI::WindowType::GUI);
    window->Relocate({screenBounds.x / 2 - 200, screenBounds.y / 2 - 200});

    GUI::Image* banner = new GUI::Image({0, 0, 400, (400 * 72 / 298)});
    banner->SetScaling(Graphics::Texture::ScaleFit);
    banner->Load("/system/lemon/resources/banner.png");

    GUI::LayoutContainer* buttonContainer = new GUI::LayoutContainer({0, 0, 0, 30}, {0, 24});
    buttonContainer->SetLayout(GUI::LayoutSize::Stretch, GUI::LayoutSize::Fixed, GUI::WAlignLeft, GUI::WAlignBottom);
    buttonContainer->xFill = true;

    GUI::Button* about = new GUI::Button("About Lemon OS...", {});
    about->e.onPress.Set(OnAboutLemon);
    GUI::Button* thirdparty = new GUI::Button("Third Party...", {});
    thirdparty->e.onPress.Set(OnThirdParty);

    buttonContainer->AddWidget(about);
    buttonContainer->AddWidget(thirdparty);

    GUI::TextBox* textbox = new GUI::TextBox({5, 105, 5, 30}, true);
    textbox->editable = false;
    textbox->SetLayout(GUI::LayoutSize::Stretch, GUI::LayoutSize::Stretch);

    std::string text;
    FILE* textFile = fopen("/system/lemon/docs/welcome.txt", "r");
    if(!textFile) {
        GUI::DisplayMessageBox("welcome.lef", "Failed to open /system/lemon/docs/welcome.txt");
    }

    size_t bytes;
    char buffer[257];
    while((bytes = fread(buffer, 1, 256, textFile))) {
        buffer[bytes] = 0;
        text += buffer; 
    }

    textbox->LoadText(text.c_str());

    window->AddWidget(banner);
    window->AddWidget(textbox);
    window->AddWidget(buttonContainer);

    while (!window->closed) {
        Lemon::WindowServer::Instance()->Poll();

        window->GUIPollEvents();
        window->Paint();

        Lemon::WindowServer::Instance()->Wait();
    }

    return 0;
}
