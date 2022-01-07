#pragma once

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "Test.h"

// This more than anything is to stress the kernel
// for any deadlocks or memory issues
int RunTerminalTest()  {
    pid_t pids[12] = {};

    const char* const argv[] = {
        "/system/bin/terminal.lef",
        nullptr
    };

    for(int i = 0; i < 12; i++){
        pid_t p = fork();

        if(p == 0){
            execvp("/system/bin/terminal.lef", const_cast<char* const*>(argv));

            exit(1); // We shouldnt be here
        } else {
            pids[i] = p;
        }
    }

    usleep(1000000);

    for(int i = 0; i < 12; i++){
        if(kill(pids[i], SIGKILL)) {
            return 1; // Process may have already died?
        }
    }

    return 0;
}

static Test termTest = {
    .func = RunTerminalTest,
    .prettyName = "Terminal open/close test",
};
