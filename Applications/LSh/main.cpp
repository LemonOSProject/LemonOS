#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <lemon/system/spawn.h>
#include <lemon/system/filesystem.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <lemon/syscall.h>
#include <string>
#include <termios.h>
#include <vector>

termios execAttributes; // Set before executing
termios readAttributes; // Set on ReadLine

std::string ln;

typedef void(*builtin_call_t)(int, char**);

typedef struct {
	const char* name;
	builtin_call_t func;
} builtin_t;

char currentDir[PATH_MAX];

std::list<std::string> path;

std::list<builtin_t> builtins;

std::vector<std::string> history;

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

void LShBuiltin_Export(int argc, char** argv){
	for(int i = 1; i < argc; i++){
		putenv(argv[i]);
	}
}

void LShBuiltin_Clear(int argc, char** argv){
	printf("\033c");
}

builtin_t builtinCd = {.name = "cd", .func = LShBuiltin_Cd};
builtin_t builtinPwd = {.name = "pwd", .func = LShBuiltin_Pwd};
builtin_t builtinExport = {.name = "export", .func = LShBuiltin_Export};
builtin_t builtinClear = {.name = "clear", .func = LShBuiltin_Clear};

void ReadLine(){
	tcsetattr(STDOUT_FILENO, TCSANOW, &readAttributes);

	bool esc = false; // Escape sequence
	bool csi = false; // Control sequence indicator

	int lnPos = 0; // Line cursor position
	int historyIndex = -1; // History index
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
					if(historyIndex < static_cast<long>(history.size()) - 1){
						historyIndex++;

						ln = history[history.size() - historyIndex - 1];

						printf("\e[%dD\e[K%s", lnPos, ln.c_str()); // Delete line and print new line

						lnPos = ln.length();
					}
					break;
				case 'B': // Cursor Down
					if(historyIndex > 0){
						historyIndex--;
						ln = history[history.size() - historyIndex - 1];
					} else {
						historyIndex = -1;
						ln.clear();
					}

					printf("\e[%dD\e[K%s", lnPos, ln.c_str()); // Delete line and print new line

					lnPos = ln.length();
					break;
				case 'C': // Cursor Right
					if(lnPos < static_cast<int>(ln.length())){
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
					printf("\e[%uC", static_cast<int>(ln.length()) - lnPos);
					lnPos = ln.length();
					break;
				case 'H': // Home
					printf("\e[%dD", lnPos);
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
			putc(ch, stdout);
			break;
		} else if(ch == '\e'){
			esc = true;
			csi = false;
		} else {
			ln.insert(lnPos, 1, ch);
			putc(ch, stdout);
			lnPos++;
		}

		if(lnPos < static_cast<int>(ln.length())){
			printf("\e[K%s", &ln[lnPos]); // Clear past cursor, print everything in front of the cursor

			for(int i = 0; i < static_cast<int>(ln.length()) - lnPos; i++){
				printf("\e[D");
			}
		}

		fflush(stdout);
	}
	fflush(stdout);

	tcsetattr(STDOUT_FILENO, TCSANOW, &execAttributes);
}

void ParseLine(){
	if(!ln.length()){
		return;
	}

	history.push_back(ln);

	int argc = 0;
	std::vector<char*> argv;

	char* lnC = strdup(ln.c_str());

	if(!lnC){
		return;
	}

	char* tok = strtok(lnC, " \t\n");
	argv.push_back(tok);
	argc++;

	while((tok = strtok(NULL, " \t\n"))){
		argv.push_back(tok);
		argc++;
	}

	for(builtin_t builtin : builtins){
		if(strcmp(builtin.name, argv[0]) == 0){
			builtin.func(argc, argv.data());
			return;
		}
	}

	int fd;
	if(strchr(argv[0], '/') && (fd = open(currentDir, O_RDONLY | O_DIRECTORY))){
		pid_t pid = lemon_spawn(argv[0], argc, argv.data(), 1);
		if(pid > 0){
			syscall(SYS_WAIT_PID, pid, 0, 0, 0, 0);
		}

		close(fd);

		if(lnC)
			free(lnC);

		close(fd);

		return;
	} else for(std::string path : path){
		if((fd = open(path.c_str(), O_RDONLY | O_DIRECTORY)) > 0){
			lemon_dirent_t dirent;

			int i = 0;
			while (lemon_readdir(fd, i++, &dirent)){
				// Check exact filenames and try omitting extension of .lef files
				if(strcmp(argv[0], dirent.name) == 0 || (strcmp(dirent.name + strlen(dirent.name) - 4, ".lef") == 0 && strncmp(argv[0], dirent.name, strlen(dirent.name) - 4) == 0)){
					path = path + "/" + dirent.name;
					
					pid_t pid = lemon_spawn(path.c_str(), argc, argv.data(), 1);

					if(pid){
						syscall(SYS_WAIT_PID, pid, 0, 0, 0, 0);
					} else {
						printf("Error executing %s\n", dirent.name);
					}

					close(fd);
					free(lnC);
					return;
				}
			}

			close(fd);
		}
	}

	printf("\nUnknown Command: %s\n", argv[0]);
	free(lnC);
}

int main(){
	printf("Lemon SHell\n");

	if(const char* h = getenv("HOME"); h){
		chdir(h);
	}

	getcwd(currentDir, PATH_MAX);
	tcgetattr(STDOUT_FILENO, &execAttributes);
	readAttributes = execAttributes;
	readAttributes.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo when reading user input 
	
	builtins.push_back(builtinPwd);
	builtins.push_back(builtinCd);
	builtins.push_back(builtinExport);
	builtins.push_back(builtinClear);

	std::string pathEnv = getenv("PATH");
	std::string temp;
	for(char c : pathEnv){
		if(c == ':' && temp.length()){
			path.push_back(temp);
			temp.clear();
		} else {
			temp += c;
		}
	}
	if(temp.length()){
		path.push_back(temp);
		temp.clear();
	}

	fflush(stdin);

	for(;;) {
		printf("\n\e[33mLemon \e[91m%s\e[m$ ", currentDir);
		fflush(stdout);

		ReadLine();
		ParseLine();
	}
}