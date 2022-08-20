#include <lemon/system/Spawn.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <list>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <stack>
#include <vector>

termios execAttributes; // Set before executing
termios readAttributes; // Set on ReadLine

std::string ln;
int lnPos = 0;
int historyIndex = -1;

typedef int (*builtin_call_t)(int, char**);

typedef struct {
    const char* name;
    builtin_call_t func;
} builtin_t;

char currentDir[PATH_MAX];

std::list<std::string> path;

std::list<builtin_t> builtins;

std::vector<std::string> history;

int LShBuiltin_Cd(int argc, char** argv) {
    if (argc >= 3) {
        printf("cd: Too many arguments");
        return 1;
    } else if (argc == 2) {
        if (chdir(argv[1])) {
            printf("cd: Error changing working directory to %s\n", argv[1]);
        }

        return 1;
    } else
        chdir("/");

    getcwd(currentDir, PATH_MAX);
    return 0;
}

int LShBuiltin_Pwd(int argc, char** argv) {
    getcwd(currentDir, PATH_MAX);
    printf("%s\n", currentDir);

    return 0;
}

int LShBuiltin_Export(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        putenv(argv[i]);
    }

    return 0;
}

int LShBuiltin_Clear(int argc, char** argv) {
    printf("\033c");
    fflush(stdout);

    return 0;
}

[[noreturn]] int LShBuiltin_Exit(int argc, char** argv) { exit(0); }

builtin_t builtinCd = {.name = "cd", .func = LShBuiltin_Cd};
builtin_t builtinPwd = {.name = "pwd", .func = LShBuiltin_Pwd};
builtin_t builtinExport = {.name = "export", .func = LShBuiltin_Export};
builtin_t builtinClear = {.name = "clear", .func = LShBuiltin_Clear};
builtin_t builtinExit = {.name = "exit", .func = LShBuiltin_Exit};

pid_t job = -1;

int commandResult = 0; // Return code of last command

void InterruptSignalHandler(int sig) {
    if (job > 0) {
        // If we have a current job, send signal to child.
        kill(job, sig);
    } else if(sig == SIGINT) {
        // Clear the line on Ctrl+C
        lnPos = 0;
        historyIndex = -1;
        ln.clear();
    }
}

void ReadLine() {
    tcsetattr(STDOUT_FILENO, TCSANOW, &readAttributes);

    bool esc = false; // Escape sequence
    bool csi = false; // Control sequence indicator

    lnPos = 0;             // Line cursor position
    historyIndex = -1; // History index
    ln.clear();

    for (int i = 0;; i++) {
        char ch;
        while ((ch = getchar()) == EOF)
            ;

        if (esc) {
            if (ch == '[') {
                csi = true;
            }
            esc = false;
        } else if (csi) {
            switch (ch) {
            case 'A': // Cursor Up
                if (historyIndex < static_cast<long>(history.size()) - 1) {
                    historyIndex++;

                    ln = history[history.size() - historyIndex - 1];

                    printf("\e[%dD\e[K%s", lnPos, ln.c_str()); // Delete line and print new line

                    lnPos = ln.length();
                }
                break;
            case 'B': // Cursor Down
                if (historyIndex > 0) {
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
                if (lnPos < static_cast<int>(ln.length())) {
                    lnPos++;
                    printf("\e[C");
                }
                break;
            case 'D': // Cursor Left
                lnPos--;
                if (lnPos < 0) {
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
        } else if (ch == '\b') {
            if (lnPos > 0) {
                lnPos--;
                ln.erase(lnPos, 1);
                putc(ch, stdout);
            }
        } else if (ch == '\n') {
            putc(ch, stdout);
            break;
        } else if (ch == '\e') {
            esc = true;
            csi = false;
        } else {
            ln.insert(lnPos, 1, ch);
            putc(ch, stdout);
            lnPos++;
        }

        if (lnPos < static_cast<int>(ln.length())) {
            printf("\e[K%s", &ln[lnPos]); // Clear past cursor, print everything in front of the cursor

            for (int i = 0; i < static_cast<int>(ln.length()) - lnPos; i++) {
                printf("\e[D");
            }
        }

        fflush(stdout);
    }
    fflush(stdout);

    tcsetattr(STDOUT_FILENO, TCSANOW, &execAttributes);
}

void ParseLine() {
    if (ln.empty()) {
        return;
    }

    enum {
        ParseNormal,
        ParseSingleQuotes,
        ParseDoubleQuotes,
        ParseEnv,
        ParseEscape,
    };

    std::stack<int> state;
    state.push(ParseNormal);

    history.push_back(ln);
    ln.push_back('\n'); // Insert a '\n' at the end

    std::vector<char*> argv;

    std::string lineBuf;
    std::string envBuf;

    auto pushArg = [&]() -> void {
        if(lineBuf.empty()) {
            return;
        }

        assert(lineBuf.c_str());

        argv.push_back(strdup(lineBuf.c_str()));
        lineBuf.clear();
    };

    auto pushEnv = [&](std::string val) -> void {
        lineBuf += std::move(val);
        envBuf.clear();
    };

    auto isLineSeperator = [](char c) -> bool { return c == ' ' || c == '\n'; };

    for (auto it = ln.begin(); it != ln.end(); it++) {
        char c = *it;
        switch (state.top()) {
        case ParseNormal:
            if (c == '\'') {
                state.push(ParseSingleQuotes);
                break;
            } else if (c == '\"') {
                state.push(ParseDoubleQuotes);
                break;
            } else if (isLineSeperator(c)) {
                pushArg();
                break;
            }

            [[fallthrough]];
        case ParseDoubleQuotes:
            if (c == '\"') {
                // The fallthrough will not matter as
                // this case is already tested for ParseNormal
                state.pop();
                break;
            }

            if (c == '\\') {
                state.push(ParseEscape);
            } else if (c == '$') {
                state.push(ParseEnv);
            } else {
                lineBuf += c;
            }
            break;
        case ParseSingleQuotes:
            if (c == '\'') { // End single quotes
                state.pop();
            } else {
                lineBuf += c;
            }
            break;
        case ParseEnv:
            if (envBuf.empty()) {
                if (c == '$') { // Shell Process ID
                    pushEnv(std::to_string(getpid()));
                    state.pop();
                } else if (c == '?') { // Return value of last command
                    pushEnv(std::to_string(commandResult));
                    state.pop();
                }
            }

            if (isalnum(c)) {
                envBuf += c;
            } else if (isLineSeperator(c) || c == '\\' || c == '\'' || c == '\"') {
                pushEnv(getenv(envBuf.c_str()));
                state.pop();
                it--; // The previous state deals with the separator
            }
            break;
        case ParseEscape:
            lineBuf += c;
            state.pop();
            break;
        }
    }

    assert(lineBuf.empty());

    if(!argv.size()) {
        return;
    }
    argv.push_back(nullptr);

    for (builtin_t builtin : builtins) {
        if (strcmp(builtin.name, argv[0]) == 0) {
            commandResult = builtin.func(argv.size() - 1, argv.data());
            return;
        }
    }

    errno = 0;

    job = fork();
    if(job == 0) {
        if (strchr(argv[0], '/')) {
            execv(argv[0], argv.data());
            if(errno == ENOENT) {
                printf("Command not found: %s\n", argv[0]);
            } else {
                printf("%s: %s\n", strerror(errno), argv[0]);
            }
            
            exit(errno);
        } else {
            for (std::string path : path) {
                assert(!path.empty());

                errno = execv((path + "/" + argv[0]).c_str(), argv.data());
            }

            if(errno == ENOENT) {
                printf("Command not found: %s\n", argv[0]);
            } else {
                printf("%s: %s\n", strerror(errno), argv[0]);
            }
            
            exit(errno);
        }
    } else if (job > 0) {
        int status = 0;
        int ret = 0;
        while ((ret = waitpid(job, &status, 0)) == 0 || (ret < 0 && errno == EINTR))
            ;

        commandResult = WEXITSTATUS(status);

        job = -1;
    } else {
        perror("Error spawning process");
    }

    job = -1;

    return;
}

int main() {
    printf("Lemon SHell\n");

    if (const char* h = getenv("HOME"); h) {
        chdir(h);
    }

    setenv("SHELL", "/system/bin/lsh", 1);

    getcwd(currentDir, PATH_MAX);
    tcgetattr(STDOUT_FILENO, &execAttributes);
    readAttributes = execAttributes;
    readAttributes.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo when reading user input

    struct sigaction action = {};
    action.sa_handler = InterruptSignalHandler;
    sigemptyset(&action.sa_mask);

    // Send both SIGINT and SIGWINCH to child
    if (sigaction(SIGINT, &action, nullptr)) {
        perror("sigaction");
        return 99;
    }

    if (sigaction(SIGWINCH, &action, nullptr)) {
        perror("sigaction");
        return 99;
    }

    builtins.push_back(builtinPwd);
    builtins.push_back(builtinCd);
    builtins.push_back(builtinExport);
    builtins.push_back(builtinClear);
    builtins.push_back(builtinExit);

    std::string pathEnv = getenv("PATH");
    std::string temp = "";
    for (char c : pathEnv) {
        if (c == ':' && !temp.empty()) {
            path.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    if (temp.length()) {
        path.push_back(temp);
        temp.clear();
    }

    fflush(stdin);

    for (;;) {
        getcwd(currentDir, PATH_MAX);
        printf("\n\e[33mLemon \e[91m%s\e[m$ ", currentDir);
        fflush(stdout);

        ReadLine();
        ParseLine();
    }
}