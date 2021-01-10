#include <lemon/system/spawn.h>
#include <lemon/system/util.h>
#include <lemon/core/json.h>
#include <lemon/core/sha.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <string>

int main(int argc, char** argv){
	setenv("HOME", "/system", 1); // Default home
	setenv("PATH", "/initrd:/system/bin", 1); // Default path

	Lemon::JSONParser confParser = Lemon::JSONParser("/system/lemon/lemond.json");
	auto json = confParser.Parse();
    if(json.IsObject()){
        std::map<Lemon::JSONKey, Lemon::JSONValue>& values = *json.object;

		if(auto it = values.find("environment"); it != values.end() && it->second.IsArray()){
			std::vector<Lemon::JSONValue>& env = *it->second.array;

			for(auto& v : env){
				std::string str;
				size_t eq = std::string::npos;
				if(v.IsString() && (eq = (str = v.AsString()).find("=")) != std::string::npos){
					setenv(str.substr(0, eq - 1).c_str(), str.substr(eq + 1).c_str(), 1);
				}
			}
		}
	} else {
        printf("[Lemond] Warning: Error parsing JSON configuration file!\n");
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
