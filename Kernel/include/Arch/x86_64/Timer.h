#pragma once

#include <stdint.h>

#include <bits/posix/timeval.h>
#include <bits/ansi/timespec.h>

typedef long time_t;

static inline bool operator<(timeval l, timeval r){
    return (l.tv_sec < r.tv_sec) || (l.tv_sec == r.tv_sec && l.tv_usec < r.tv_usec);
}

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

inline long operator-(const timeval& l, const timeval& r){
    return Timer::TimeDifference(l, r);
}