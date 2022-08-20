#include <Lemon/System/Spawn.h>
#include <Lemon/System/Util.h>
#include <Lemon/Core/JSON.h>
#include <Lemon/Core/SHA.h>

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

#include <string>
#include <vector>
#include <list>

struct Service {
	std::string name;
	std::string target;

	enum {
		StateStarting,
		StateRunning,
		StateError,
		StateStopped,
	} state = StateStarting;

	pid_t pid = -1;
};

std::multimap<std::string, Service> waitingServices;
std::list<Service> services;

void StartService(Service& srv){
	char* const argv[] = { (char*) srv.name.c_str() };
	pid_t pid = lemon_spawn(srv.target.c_str(), 1, argv, 1);
	if(pid <= 0){
		printf("[lemond] Error: Failed to start '%s'!\n", srv.name.c_str());
		return;
	}

	srv.pid = pid;

	auto range = waitingServices.equal_range(srv.name);
	for(auto it = range.first; it != range.second; it++){
		services.push_back(it->second);
		StartService(it->second);
	}
}

int main(int, char**){
	setenv("HOME", "/system", 1); // Default home
	setenv("PATH", "/system/bin:/system/lemon:/initrd", 1); // Default path

	Lemon::JSONParser confParser = Lemon::JSONParser("/system/lemon/lemond.json");
	auto json = confParser.Parse();
    if(json.IsObject()){
        std::map<Lemon::JSONKey, Lemon::JSONValue>& values = *json.data.object;

		if(auto it = values.find("environment"); it != values.end() && it->second.IsArray()){
			std::vector<Lemon::JSONValue>& env = *it->second.data.array;

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
			auto& values = *root.data.object;

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

	while(1){
		if(pid_t pid = waitpid(-1, nullptr, 0); pid > 0){
			for(auto it = services.begin(); it != services.end(); it++){
				Service& svc = *it;
				if(svc.pid == pid){
					printf("[lemond] Warning: '%s' (pid %d) closed.\n", svc.name.c_str(), svc.pid);
				}
				
				services.erase(it);
				break;
			}
		}
	}
}
