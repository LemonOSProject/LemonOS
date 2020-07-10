#pragma once

#include <stdint.h>

typedef struct {
    long seconds;
    long milliseconds;
} timeval_t;

namespace Timer{

    timeval_t GetSystemUptimeStruct();
    int TimeDifference(timeval_t& newTime, timeval_t& oldTime);

    uint64_t GetSystemUptime();
    uint32_t GetTicks();
    uint32_t GetFrequency();

    // Initialize
    void Initialize(uint32_t freq);
}