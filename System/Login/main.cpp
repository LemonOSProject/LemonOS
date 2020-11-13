#include <gui/window.h>
#include <core/cfgparser.h>
#include <gui/messagebox.h>
#include <stdexcept>
#include <core/sha.h>
#include <unistd.h>
#include <lemon/spawn.h>

Lemon::GUI::Window* window;

Lemon::GUI::Label* usernameLabel;
Lemon::GUI::TextBox* usernameBox;

Lemon::GUI::Label* passwordLabel;
Lemon::GUI::TextBox* passwordBox;

Lemon::GUI::Button* okButton;

struct User{
	std::string username;
	std::string hash;
	int uid;
	int gid;
};

std::map<std::string, User> users;

void OnOKPress(__attribute__((unused)) Lemon::GUI::Button* b){
	try{
		User& user = users.at(usernameBox->contents.front());

		SHA256 passwordHash;
		passwordHash.Update(passwordBox->contents.front().data(), passwordBox->contents.front().length());

		if(user.hash.compare(passwordHash.GetHash())){
			char buf[128];
			printf("Actual hash: %s, inserted hash: %s\n", user.hash.c_str(), passwordHash.GetHash().c_str());
			snprintf(buf, 128, "Incorrect password for '%s'!", usernameBox->contents.front().c_str());
			Lemon::GUI::DisplayMessageBox("Incorrect Password", buf);
			return;
		}

		setuid(user.uid);
		setgid(user.gid);

		char* const args[] = {const_cast<char*>("/system/bin/shell.lef")};
		lemon_spawn(*args, 1, args, 0);

		delete window;

		exit(0);
	} catch (std::out_of_range& e){
		char buf[100];
		snprintf(buf, 128, "Unknown user '%s'", usernameBox->contents.front().c_str());
		Lemon::GUI::DisplayMessageBox("Invalid Username", buf);
		return;
	}
}

int main(int argc, char** argv){
	CFGParser cfgParser = CFGParser("/system/lemon/users.cfg");

	cfgParser.Parse();
	auto items = cfgParser.GetItems();

	for(auto user : items){
		if(user.first.compare("user")){
			printf("users.cfg: Unknown heading [%s]\n", user.first.c_str());
			continue;
		}

		int uid = 0;
		int gid = 0;
		std::string username, hash;

		for(auto entry : user.second){
			if(!entry.name.compare("name")){
				username = entry.value;
			} else if(!entry.name.compare("uid")){
				uid = std::stoi(entry.value);
			} else if(!entry.name.compare("gid")){
				gid = std::stoi(entry.value);
			} else if(!entry.name.compare("hash")){
				hash = entry.value;
			} else {
				printf("users.cfg: Unknown entry: %s\n", entry.name.c_str());
			}
		}

		if(uid > 0 && gid > 0 && username.length()){
			printf("Found user: Name is %s, UID is %d, GID is %d\n", username.c_str(), uid, gid);

			users[username] = {.username = username, .hash = hash, .uid = uid, .gid = gid};
		}
	}

	usernameLabel = new Lemon::GUI::Label("Username:", {10, 20, 100, 16});
	usernameBox = new Lemon::GUI::TextBox({Lemon::Graphics::GetTextLength("Username:") + 20, 16, 10, 24}, false);
	usernameBox->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Fixed);

	passwordLabel = new Lemon::GUI::Label("Password:", {10, 48, 100, 16});
	passwordBox = new Lemon::GUI::TextBox({Lemon::Graphics::GetTextLength("Username:") + 20, 44, 10, 24}, false);
	passwordBox->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Fixed);
	passwordBox->MaskText(true);

	okButton = new Lemon::GUI::Button("OK", {0, 10, 100, 24});
	okButton->SetLayout(Lemon::GUI::Fixed, Lemon::GUI::Fixed, Lemon::GUI::WAlignCentre, Lemon::GUI::WAlignBottom);
	okButton->OnPress = OnOKPress;
	
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