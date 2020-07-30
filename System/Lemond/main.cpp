#include <lemon/spawn.h>
#include <lemon/util.h>
#include <core/cfgparser.h>
#include <core/sha.h>
#include <string.h>
#include <core/rotate.h>
#include <core/msghandler.h>
#include <core/systemservice.h>
#include <vector>
#include <core/message.h>

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
	
	return 0;

	for(;;){
		Lemon::Yield();
	}
}