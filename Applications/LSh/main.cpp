#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include <lemon/spawn.h>
#include <lemon/filesystem.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <lemon/syscall.h>
#include <string>
#include <termios.h>

termios execAttributes; // Set before executing
termios readAttributes; // Set on ReadLine

std::string ln;

typedef void(*builtin_call_t)(int, char**);

typedef struct {
	char* name;
	builtin_call_t func;
} builtin_t;

char currentDir[PATH_MAX];

List<char*> path;

List<builtin_t> builtins;

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

builtin_t builtinCd = {.name = "cd", .func = LShBuiltin_Cd};
builtin_t builtinPwd = {.name = "pwd", .func = LShBuiltin_Pwd};

void ReadLine(){
	tcsetattr(STDOUT_FILENO, TCSANOW, &readAttributes);

	bool esc = false;
	bool csi = false;
	int lnPos = 0;
	ln.clear();

	for(int i = 0; ; i++){
		char ch;
		while((ch = getchar()) == EOF);

		if(esc){
			if(ch == '['){
				csi = true;
			}
			esc = false;
		} else if (csi) {
			switch(ch){
				case 'A': // Cursor Up
					break;
				case 'B': // Cursor Down
					break;
				case 'C': // Cursor Right
					if(lnPos < ln.length()){
						lnPos++;
						printf("\e[C");
					}
					break;
				case 'D': // Cursor Left
					lnPos--;
					if(lnPos < 0){
						lnPos = 0;
					} else {
						printf("\e[D");
					}
					break;
				case 'F': // End
					lnPos = ln.length();
					break;
				case 'H': // Home
					lnPos = 0;
					break;
			}
			csi = false;
		} else if(ch == '\b'){
			if(lnPos > 0){
				lnPos--;
				ln.erase(lnPos, 1);
				putc(ch, stdout);
			}
		} else if(ch == '\n'){
			break;
		} else if(ch == '\e'){
			esc = true;
			csi = false;
		} else {
			ln.insert(lnPos, 1, ch);
			putc(ch, stdout);
			lnPos++;
		}

		if(lnPos < ln.length()){
			printf("\e[K%s", &ln[lnPos]); // Clear past cursor, print everything in front of the cursor

			for(int i = 0; i < ln.length() - lnPos; i++){
						printf("\e[D");
			}
		}

		fflush(stdout);
	}

	tcsetattr(STDOUT_FILENO, TCSANOW, &execAttributes);
}

void ParseLine(){
	int argc = 0;
	char* argv[128];

	char* lnC = new char[ln.size() + 1];
	std::copy(ln.begin(), ln.end(), lnC);
	char* tok = strtok(lnC, " \t\n");
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
	if(fd = open(currentDir, O_RDONLY | O_DIRECTORY)){
		lemon_dirent_t dirent;

		int i = 0;
		while (lemon_readdir(fd, i++, &dirent)){
			if(strcmp(argv[0], dirent.name) == 0){
				pid_t pid = lemon_spawn(dirent.name, argc, argv, 1);

				syscall(SYS_WAIT_PID, pid, 0, 0, 0, 0);

				close(fd);
				return;
			}
		}

		close(fd);
	}
	
	for(int j = 0; j < path.get_length(); j++){
		if((fd = open(path.get_at(j), O_RDONLY | O_DIRECTORY)) > 0){
			lemon_dirent_t dirent;

			int i = 0;
			while (lemon_readdir(fd, i++, &dirent)){
				// Check exact filenames and try omitting extension of .lef files
				if(strcmp(argv[0], dirent.name) == 0 || (strcmp(dirent.name + strlen(dirent.name) - 4, ".lef") == 0 && strncmp(argv[0], dirent.name, strlen(dirent.name) - 4) == 0)){
					char* tempPath = (char*)malloc(strlen(path.get_at(j)) + strlen(dirent.name) + 2);
					strcpy(tempPath, path.get_at(j));
					strcat(tempPath, "/");
					strcat(tempPath, dirent.name);
					
					pid_t pid = lemon_spawn(tempPath, argc, argv, 1);

					if(pid){
						syscall(SYS_WAIT_PID, pid, 0, 0, 0, 0);
					} else {
						printf("Error executing %s\n", dirent.name);
					}

					close(fd);
					free(tempPath);
					return;
				}
			}

			close(fd);
		}
	}

	printf("\nUnknown Command: %s\n", argv[0]);
}

int main(){
	printf("Lemon SHell\n");

	getcwd(currentDir, PATH_MAX);
	tcgetattr(STDOUT_FILENO, &execAttributes);
	readAttributes = execAttributes;
	readAttributes.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo when reading user input 
	
	builtins.add_back(builtinCd);
	builtins.add_back(builtinPwd);

	path.add_back("/initrd");
	path.add_back("/initrd/bin");
	path.add_back("/system/bin");
	path.add_back("/system");

	fflush(stdin);

	for(;;) {
		printf("\n\e[33mLemon \e[91m%s\e[m$ ", currentDir);
		fflush(stdout);

		ReadLine();
		ParseLine();
	}
}