#include <lemon/system/spawn.h>
#include <lemon/system/util.h>
#include <lemon/core/cfgparser.h>
#include <lemon/core/sha.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <string>

int main(int argc, char** argv){
	setenv("HOME", "/system", 1); // Default home
	setenv("PATH", "/initrd:/system/bin", 1); // Default path

	CFGParser confParser = CFGParser("/system/lemon/lemond.cfg");
	confParser.Parse();
	for(auto& heading : confParser.GetItems()){
		if(!heading.first.compare("Environment")){
			for(auto& env : heading.second){
				setenv(env.name.c_str(), env.value.c_str(), 1); // Name, Value, Replace
			}
		}
	}

	__attribute__((unused)) char* lemonwm = "/system/lemon/lemonwm.lef";
	__attribute__((unused)) char* login = "/system/lemon/login.lef";
	__attribute__((unused)) char* shell = "/system/bin/shell.lef";

	if(long ret = lemon_spawn(lemonwm, 1, &lemonwm); ret <= 0){
		printf("Error %ld attempting to load %s. Attempting to load again\n", ret, lemonwm);
		lemon_spawn(lemonwm, 1, &lemonwm); // Attempt twice
	}
		
	if(long ret = lemon_spawn(shell, 1, &shell); ret <= 0){
		printf("Error %ld attempting to load %s. Attempting to load again\n", ret, shell);
		lemon_spawn(shell, 1, &shell); // Attempt twice
	}
	
	return 0;
}
