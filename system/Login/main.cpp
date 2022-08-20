#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Messagebox.h>

#include <Lemon/Core/JSON.h>
#include <Lemon/Core/SHA.h>

#include <Lemon/System/Spawn.h>

#include <stdexcept>
#include <unistd.h>

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

void OnOKPress(void*){
	if(users.find(usernameBox->contents.front()) != users.end()){
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
	} else {
		char buf[128];
		snprintf(buf, 128, "Unknown user '%s'", usernameBox->contents.front().c_str());
		Lemon::GUI::DisplayMessageBox("Invalid Username", buf);
		return;
	}
}

int main(int, char**){
	Lemon::JSONParser cfgParser = Lemon::JSONParser("/system/lemon/users.json");

	auto json = cfgParser.Parse();
	if(json.IsObject()){
        std::map<Lemon::JSONKey, Lemon::JSONValue>& root = *json.data.object;

		if(auto it = root.find("users"); it != root.end() && it->second.IsArray()){
			for(Lemon::JSONValue& v : *it->second.data.array){
				if(v.IsObject()){
					User u;
					std::map<Lemon::JSONKey, Lemon::JSONValue>& values = *v.data.object;

					if(auto it = values.find("name"); it != values.end() && it->second.IsString()){
						u.username = it->second.AsString();
					} else {
						continue;
					}

					if(auto it = values.find("hash"); it != values.end() && it->second.IsString()){
						u.hash = it->second.AsString();
					} else {
						continue;
					}

					if(auto it = values.find("uid"); it != values.end() && it->second.IsNumber()){
						u.uid = it->second.AsSignedNumber();
					} else {
						continue;
					}

					if(auto it = values.find("gid"); it != values.end() && it->second.IsNumber()){
						u.gid = it->second.AsSignedNumber();
					} else {
						continue;
					}

					users[u.username] = u;
				}
			}
		}
	} else {
		printf("Failied to parse users.json!\n");
	}

	if(!users.size()){ // TODO: Don't just boot to shell
		char* const args[] = {const_cast<char*>("/system/bin/shell.lef")}; // There are no users just load the shell
		lemon_spawn(*args, 1, args, 0);

		exit(0);
	}

	usernameLabel = new Lemon::GUI::Label("Username:", {10, 20, 100, 16});
	usernameBox = new Lemon::GUI::TextBox({Lemon::Graphics::GetTextLength("Username:") + 20, 16, 10, 24}, false);
	usernameBox->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Fixed);
	usernameBox->LoadText("root");

	passwordLabel = new Lemon::GUI::Label("Password:", {10, 48, 100, 16});
	passwordBox = new Lemon::GUI::TextBox({Lemon::Graphics::GetTextLength("Username:") + 20, 44, 10, 24}, false);
	passwordBox->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Fixed);
	passwordBox->MaskText(true);

	okButton = new Lemon::GUI::Button("OK", {0, 10, 100, 24});
	okButton->SetLayout(Lemon::GUI::Fixed, Lemon::GUI::Fixed, Lemon::GUI::WAlignCentre, Lemon::GUI::WAlignBottom);
	okButton->e.onPress.Set(OnOKPress);
	
	window = new Lemon::GUI::Window("Login", {400, 120}, 0, Lemon::GUI::WindowType::GUI);

	window->AddWidget(usernameLabel);
	window->AddWidget(usernameBox);
	window->AddWidget(passwordLabel);
	window->AddWidget(passwordBox);
	window->AddWidget(okButton);
	
	while(1){
		Lemon::WindowServer::Instance()->Poll();

		window->GUIPollEvents();
		window->Paint();
        Lemon::WindowServer::Instance()->Wait();
	}

	return -1;
}