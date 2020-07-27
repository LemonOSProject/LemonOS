#pragma once

#include <stdint.h>

typedef struct {
    long seconds;
    long milliseconds;
} timeval_t;

struct thread;

namespace Timer{

    timeval_t GetSystemUptimeStruct();
    int TimeDifference(timeval_t& newTime, timeval_t& oldTime);

    uint64_t GetSystemUptime();
    uint32_t GetTicks();
    uint32_t GetFrequency();

    void SleepCurrentThread(timeval_t& time);
    void SleepCurrentThread(long ticks);

    // Initialize
    void Initialize(uint32_t freq);
}