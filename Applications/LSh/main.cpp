#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include <lemon/spawn.h>
#include <lemon/filesystem.h>
#include <unistd.h>
#include <fcntl.h>

size_t bufSz = 512;
char* ln = nullptr;

typedef void(*builtin_call_t)(int, char**);

typedef struct {
	char* name;
	builtin_call_t func;
} builtin_t;

char currentDir[1024];

List<builtin_t> builtins;

void LShBuiltin_LS(int argc, char** argv){

	int opt;
	while((opt = getopt(argc, argv, "h")) && opt != -1){
		switch(opt){
			case 'h':
				printf("Usage: %s [options] [directory]\n\nOptions:\n    -h     Show this help\n", argv[0]);
				break;
			case '?':
				printf("Unknown Option: %c, ls -h for help\n", optopt);
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

builtin_t builtinLs = {.name = "ls", .func = LShBuiltin_LS};

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

	printf("Unknown Command: %s\n", argv[0]);
}

int main(){
	printf("Lemon SHell\n");

	strcpy(currentDir, "/");

	ln = (char*)malloc(bufSz);

	builtins.add_back(builtinLs);

	for(;;) {
		printf("$ Lemon %s>", currentDir);

		ReadLine();
		ParseLine();
	}
}