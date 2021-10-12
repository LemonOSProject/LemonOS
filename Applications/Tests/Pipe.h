#pragma once

#include <lemon/syscall.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "Test.h"

namespace PipeTest {

uint8_t* entropyBuffer = nullptr;

void WriteChild(int pipeFd){
    write(pipeFd, entropyBuffer, 256);
}

};

int RunPipeTest(){
    using namespace PipeTest;
    entropyBuffer = new uint8_t[256];

    uint8_t recieveBuffer[256];
    for(int i = 0; i < 32; i++){
        if(syscall(SYS_GETENTROPY, entropyBuffer, 256)){
            printf("Failed to fill entropy buffer!\n");
            return -1;
        }

        int pipefds[2];
        if(pipe(pipefds)){
            printf("Failed to create pipe!\n");
            return -1;
        }

        if(fork() == 0){
            WriteChild(pipefds[1]);
            exit(0);
        }

        if(ssize_t r = read(pipefds[0], recieveBuffer, 256); r != 256){
            if(r > 0 && r < 256) {
                printf("Invalid data length reading pipe!\n");
                return -1;
            }

            printf("Failed to read from pipe: %s\n", strerror(errno));
            return -1;
        }

        if(memcmp(entropyBuffer, recieveBuffer, 256)){
            printf("Read unexpected data from pipe!\n");
            return -1;
        }

        close(pipefds[0]);
        close(pipefds[1]);
    }

    return 0;
}

static Test pipeTest = {
    .func = RunPipeTest,
    .prettyName = "POSIX Pipe Test",
};
