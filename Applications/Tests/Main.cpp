#include "Test.h"

#include <unordered_map>

#include <sys/wait.h>
#include <unistd.h>

#include "Audio.h"
#include "Pipe.h"
#include "Terminal.h"
#include "Syscall.h"

const std::unordered_map<std::string, Test> tests = {
    {"pipe", pipeTest},
    {"terminal", termTest},
    {"audio", audioTest},
    {"syscall", syscallTest},
};

void ExecuteTest(const Test& test) {
    int waitStatus = 0;
    pid_t child = fork();
    if (!child) {
        exit(test.func());
    } else {
        waitpid(child, &waitStatus, 0);
        if (WEXITSTATUS(waitStatus)) {
            printf("[\033[91mFAIL\033[0m] %s\n", test.prettyName.c_str());
        } else {
            printf("[\033[92m OK \033[0m] %s\n", test.prettyName.c_str());
        }
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        for (auto& test : tests) {
            ExecuteTest(test.second);
        }
    } else {
        ExecuteTest(tests.at(argv[1]));
    }

    return 0;
}
