#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include <lemon/spawn.h>
#include <lemon/filesystem.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

size_t bufSz = 512;
char* ln = nullptr;

typedef void(*builtin_call_t)(int, char**);

typedef struct {
	char* name;
	builtin_call_t func;
} builtin_t;

char currentDir[PATH_MAX];

List<builtin_t> builtins;

void LShBuiltin_Ls(int argc, char** argv){

	int opt;
	while((opt = getopt(argc, argv, "h")) && opt != -1){
		switch(opt){
			case 'h':
				printf("Usage: %s [options] [directory]\n\nOptions:\n    -h     Show this help\n", argv[0]);
				break;
			case '?':
				printf("ls :Unknown Option: %c, ls -h for help\n", optopt);
				break;
		}
	}

	char* dir = NULL;
	for(int i = optind; i < argc; i++){
		dir = argv[i];
	}

	if(!dir) dir = currentDir;

	int fd;
	if(fd = lemon_open(dir, O_RDONLY | O_DIRECTORY)){
		lemon_dirent_t dirent;

		int i = 0;
		while (lemon_readdir(fd, i++, &dirent)){
			printf("    %s%c\n", dirent.name, (dirent.type | FS_NODE_DIRECTORY) ? '/' : '\0');
		}

		close(fd);
	} else {
		printf("Could not open %s\n", dir);
	}
}

void LShBuiltin_Cd(int argc, char** argv){
	if(argc > 3){
		printf("cd: Too many arguments");
	} else if (argc == 2){
		if(chdir(argv[1])){
			printf("cd: Error changing working directory to %s\n", argv[1]);
		}
	} else chdir("/");
	
	getcwd(currentDir, PATH_MAX);
}

void LShBuiltin_Pwd(int argc, char** argv){
	getcwd(currentDir, PATH_MAX);
	printf("%s\n", currentDir);
}

builtin_t builtinLs = {.name = "ls", .func = LShBuiltin_Ls};
builtin_t builtinCd = {.name = "cd", .func = LShBuiltin_Cd};
builtin_t builtinPwd = {.name = "pwd", .func = LShBuiltin_Pwd};

char* ReadLine(){
	for(int i = 0; ; i++){
		if(i >= bufSz){
			bufSz += 64;
			ln = (char*)realloc(ln, bufSz);
		}

		char ch = getchar();
		if(ch == '\n' || ch == EOF){
			ln[i] = 0;
			break;
		} else {
			ln[i] = ch;
		}
	}
	
	return ln;
}

void ParseLine(){
	int argc = 0;
	char* argv[128];

	char* tok = strtok(ln, " \t\n");
	argv[argc++] = tok;

	while((tok = strtok(NULL, " \t\n")) && argc < 128){
		argv[argc++] = tok;
	}

	for(int i = 0; i < builtins.get_length(); i++){
		if(strcmp(builtins.get_at(i).name, argv[0]) == 0){
			builtins.get_at(i).func(argc, argv);
			return;
		}
	}

	int fd;
	if(fd = lemon_open(currentDir, O_RDONLY | O_DIRECTORY)){
		lemon_dirent_t dirent;

		int i = 0;
		while (lemon_readdir(fd, i++, &dirent)){
			if(strcmp(argv[0], dirent.name) == 0){
				lemon_spawn(dirent.name, argc, argv, 1);
			}
		}

		close(fd);
	}
	

	printf("Unknown Command: %s\n", argv[0]);
}

int main(){
	printf("Lemon SHell\n");

	getcwd(currentDir, PATH_MAX);

	ln = (char*)malloc(bufSz);

	builtins.add_back(builtinLs);
	builtins.add_back(builtinCd);
	builtins.add_back(builtinPwd);

	for(;;) {
		printf("$ Lemon %s>", currentDir);

		ReadLine();
		ParseLine();
	}
}