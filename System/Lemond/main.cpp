#include <Lemon/System/Spawn.h>
#include <Lemon/System/Util.h>
#include <Lemon/Core/JSON.h>
#include <Lemon/Core/SHA.h>

#include <string.h>
#include <vector>
#include <unistd.h>
#include <dirent.h>

#include <string>

struct Service {
	std::string name;
	std::string target;

	enum {
		StateStarting,
		StateRunning,
		StateError,
		StateStopped,
	} state = StateStarting;
};

std::multimap<std::string, Service> waitingServices;
std::vector<Service> services;

void StartService(const Service& srv){
	char* const argv[] = { (char*) srv.target.c_str() };
	lemon_spawn(srv.target.c_str(), 1, argv, 0);

	auto range = waitingServices.equal_range(srv.name);
	for(auto it = range.first; it != range.second; it++){
		StartService(it->second);
	}
}

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

	DIR* dir = opendir("/system/lemon/lemond");
	if(!dir){
		perror("[Lemond] Error: Failed to open '/system/lemon/lemond'");

		return 1;
	}

	chdir("/system/lemon/lemond");

	errno = 0;
	while(dirent* ent = readdir(dir)){
		if(ent->d_type != DT_REG){
			continue;
		}

		FILE* f = fopen(ent->d_name, "r"); // Attempt to open service file
		if(!f){
			printf("[Lemond] Warning: Error opening '%s'\n", ent->d_name);
			continue;
		}

		fseek(f, 0, SEEK_END);
		if(ftell(f) > 4096){
			printf("[Lemond] Warning: Abnormally large service file '%s', should not be >4K\n", ent->d_name);
			fclose(f);
			continue;
		}
		fseek(f, 0, SEEK_SET);

		char buffer[4097];

		unsigned len = fread(buffer, 1, 4096, f); // there is no reason why a target file should be more than 4K
		buffer[len] = 0;

		fclose(f);

		std::string_view sv = { buffer };

		Lemon::JSONParser json(sv);

		auto root = json.Parse();
		if(root.IsObject()){
			auto& values = *root.object;

			std::string name;
			std::string target;
			if(auto it = values.find("target"); it == values.end() || !it->second.IsString()){ // Check for valid name field
				printf("[Lemond] Warning: Empty or invalid target for '%s'\n", ent->d_name);
				continue;
			} else if(auto it = values.find("name"); it == values.end() || !it->second.IsString()){ // Check for valid target field
				printf("[Lemond] Warning: Empty or invalid name for '%s'\n", ent->d_name);
				continue;
			}

			Service srv;

			srv.name = values.at("name").AsString();
			srv.target = values.at("target").AsString();
			if(auto it = values.find("after"); it != values.end() && it->second.IsString()){ // The service is waiting for another
				std::string after = it->second.AsString();
				waitingServices.insert({ std::move(after), std::move(srv) });
			} else {
				services.push_back(std::move(srv));
			}
		}
	}

	if(errno){
		perror("[Lemond] Error reading '/system/lemon/lemond'");
		
		return 2;
	}

	for(auto& srv : services){
		StartService(srv);
	}
	
	return 0;
}
