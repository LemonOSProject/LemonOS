#pragma once

#include <stdint.h>

#include <abi/time.h>

typedef long time_t;

namespace Timer{
    uint64_t GetSystemUptime();
    uint64_t UsecondsSinceBoot();
    uint32_t GetFrequency();

    timeval GetSystemUptimeStruct();
    long TimeDifference(const timeval& newTime, const timeval& oldTime);

    void Wait(long ms);

    void SleepCurrentThread(timeval& time);

    // Initialize
    void Initialize(uint32_t freq);
}
