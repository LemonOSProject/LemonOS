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

	lemon_spawn("/system/lemon/lemonwm.lef", 1, (char* const[1]){"/system/lemon/lemonwm.lef"});
	lemon_spawn("/system/bin/shell.lef", 1, (char* const[1]){"/system/bin/shell.lef"});
	//lemon_spawn("/system/lemon/login.lef", 1, (char* const[1]){"/system/lemon/login.lef"});
	
	return 0;
}
