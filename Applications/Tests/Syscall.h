#pragma once

#include "Test.h"

#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <lemon/syscall.h>

inline static timespec operator-(const timespec& l, const timespec& r) {
    return {l.tv_sec - r.tv_sec, l.tv_nsec - r.tv_nsec};
}

inline static long uSecondsFromTimespec(const timespec& t) {
    return t.tv_sec * 1000000 - t.tv_nsec / 1000;
}

int RunSyscallTest() {
    int tempFile = open("/tmp/syscallbenchmark", O_RDWR | O_CREAT);
    if(tempFile < 0) {
        perror("/tmp/syscallbenchmark: ");
        return 1;
    }

    char buf[256];

    timespec t1;
    timespec t2;
    long writeTime = 0;

    for(int i = 0; i < 5; i++) {
        clock_gettime(CLOCK_BOOTTIME, &t1);
        write(tempFile, buf, 0);
        clock_gettime(CLOCK_BOOTTIME, &t2);
        writeTime += uSecondsFromTimespec(t2 - t1);
    }

    writeTime /= 5;

    close(tempFile);
    unlink("/tmp/syscallbenchmark");

    printf("empty write: avg %ld us\n", writeTime);
    return 0;
}

static Test syscallTest = { 
    .func = RunSyscallTest,
    .prettyName = "Syscall Benchmark"
};
