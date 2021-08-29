#pragma once

#include <abi-bits/pid_t.h>

typedef struct LemonProcessInfo {
    pid_t pid; // Process ID

    uint32_t threadCount; // Process Thread Count

    int32_t uid; // User ID
    int32_t gid; // Group ID

    uint8_t state; // Process State

    char name[256]; // Process Name

    uint64_t runningTime; // Amount of time in seconds that the process has been running
    uint64_t activeUs;
    bool isCPUIdle = false; // Whether or not the process is an idle process

    uint64_t usedMem; // Used memory in KB
} lemon_process_info_t;
