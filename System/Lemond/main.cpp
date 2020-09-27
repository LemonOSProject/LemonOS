#include <lemon/spawn.h>
#include <lemon/util.h>
#include <core/cfgparser.h>
#include <core/sha.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <string>

int main(int argc, char** argv){
	putenv("HOME=/system"); // Default home
	putenv("PATH=/initrd:/system/bin"); // Default path

	CFGParser confParser = CFGParser("/system/lemon/lemond.cfg");
	confParser.Parse();
	for(auto& heading : confParser.GetItems()){
		if(!heading.first.compare("Environment")){
			for(auto& env : heading.second){
				setenv(env.name.c_str(), env.value.c_str(), 1); // Name, Value, Replace
			}
		}
	}

	char* lemonwm = "/system/lemon/lemonwm.lef";
	char* login = "/system/lemon/login.lef";
	lemon_spawn(lemonwm, 1, &lemonwm);
	lemon_spawn(login, 1, &lemonwm);
	
	return 0;
}
