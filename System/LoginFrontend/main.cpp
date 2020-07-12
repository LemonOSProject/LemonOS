#include <gui/window.h>

Lemon::GUI::Window* window;

Lemon::GUI::Label* usernameLabel;
Lemon::GUI::TextBox* usernameBox;

Lemon::GUI::Label* passwordLabel;
Lemon::GUI::TextBox* passwordBox;

Lemon::GUI::Button* okButton;

int main(int argc, char** argv){
	usernameLabel = new Lemon::GUI::Label("Username:", {10, 20, 100, 16});
	usernameBox = new Lemon::GUI::TextBox({Lemon::Graphics::GetTextLength("Username:") + 20, 16, 10, 24}, false);
	usernameBox->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Fixed);

	passwordLabel = new Lemon::GUI::Label("Password:", {10, 48, 100, 16});
	passwordBox = new Lemon::GUI::TextBox({Lemon::Graphics::GetTextLength("Username:") + 20, 44, 10, 24}, false);
	passwordBox->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Fixed);

	okButton = new Lemon::GUI::Button("OK", {0, 10, 100, 24});
	okButton->SetLayout(Lemon::GUI::Fixed, Lemon::GUI::Fixed, Lemon::GUI::WAlignCentre, Lemon::GUI::WAlignBottom);
	
	window = new Lemon::GUI::Window("Login", {400, 120}, 0, Lemon::GUI::WindowType::GUI);

	window->AddWidget(usernameLabel);
	window->AddWidget(usernameBox);
	window->AddWidget(passwordLabel);
	window->AddWidget(passwordBox);
	window->AddWidget(okButton);

	while(1){
		Lemon::LemonEvent ev;
		while (window->PollEvent(ev)) {
			window->GUIHandleEvent(ev);
		}
		
		window->Paint();
		window->WaitEvent();
	}

	return -1;
}