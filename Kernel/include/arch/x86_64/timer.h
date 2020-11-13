#pragma once

#include <stdint.h>

typedef long time_t;

typedef struct {
    long seconds;
    long milliseconds;
} timeval_t;

static inline bool operator<(timeval_t l, timeval_t r){
    return (l.seconds < r.seconds) || (l.seconds == r.seconds && l.milliseconds < r.milliseconds);
}

typedef struct timespec {
	time_t tv_sec;
	long tv_nsec;
} timespec_t;

struct thread;

namespace Timer{

    timeval_t GetSystemUptimeStruct();
    int TimeDifference(timeval_t newTime, timeval_t oldTime);

    uint64_t GetSystemUptime();
    uint32_t GetTicks();
    uint32_t GetFrequency();

    void Wait(long ms);

    void SleepCurrentThread(timeval_t& time);
    void SleepCurrentThread(long ticks);

    // Initialize
    void Initialize(uint32_t freq);
}
