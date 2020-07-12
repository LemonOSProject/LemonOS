#include <lemon/spawn.h>
#include <core/cfgparser.h>
#include <core/sha.h>
#include <string.h>
#include <core/rotate.h>
#include <core/msghandler.h>
#include <core/systemservice.h>
#include <vector>

class LemonUser{
	std::string username;
	int uid, gid;
public:
	LemonUser(std::string uname, int uid, int gid){
		username = uname;
		this->uid = uid;
		this->gid = gid;
	}

	bool operator==(int uid){
		if(this->uid == uid){
			return true;
		} else {
			return false;
		}
	}
};

std::vector<LemonUser> users;

int main(int argc, char** argv){
    sockaddr_un srvAddr;
    strcpy(srvAddr.sun_path, Lemon::systemServiceSocketAddress);
    srvAddr.sun_family = AF_UNIX;
	Lemon::MessageServer systemServer = Lemon::MessageServer(srvAddr, sizeof(sockaddr_un));

	lemon_spawn("/system/lemon/lemonwm.lef", 1, (char* const[1]){"/system/lemon/lemonwm.lef"});
	lemon_spawn("/system/bin/shell.lef", 1, (char* const[1]){"/system/bin/shell.lef"});
	//lemon_spawn("/system/lemon/login.lef", 1, (char* const[1]){"/system/lemon/login.lef"});

	CFGParser cfgParser = CFGParser("/system/lemon/users.cfg");

	cfgParser.Parse();
	auto items = cfgParser.GetItems();

	for(auto user : items){
		if(user.first.compare("user")){
			printf("users.cfg: Unknown heading [%s]\n", user.first.c_str());
			continue;
		}

		int uid, gid;
		std::string username;

		for(auto entry : user.second){
			if(!entry.name.compare("name")){
				username = entry.value;
			} else if(!entry.name.compare("uid")){
				uid = std::stoi(entry.value);
			} else if(!entry.name.compare("gid")){
				gid = std::stoi(entry.value);
			} else {
				printf("users.cfg: Unknown entry: %s\n", entry.name.c_str());
			}
		}

		printf("Found user: Name is %s, UID is %d, GID is %d\n", username.c_str(), uid, gid);

		users.push_back(LemonUser(username, uid, gid));
	}

	for(;;){
		while(auto m = systemServer.Poll()){
			if(m->msg.protocol == LEMON_MESSAGE_PROTOCOL_SESSIONCMD && m->msg.length >= sizeof(Lemon::SessionCommand)){
				Lemon::SessionCommand* sCmd = (Lemon::SessionCommand*)m->msg.data;

				if(sCmd->cmd == Lemon::LemonSystemLaunchUserSession){
					for(LemonUser& user : users){
						if(user == sCmd->id){
							
						}
					}
				}
			}
		}
	}
}