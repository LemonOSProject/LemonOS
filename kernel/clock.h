#pragma once

#include <stdint.h>

#include <le/callback.h>

class ClockDevice {
public:
    virtual void start_timer(uint64_t ns) = 0;
    virtual void stop_timer() = 0;

    virtual uint64_t ns_since_boot() = 0;
    virtual const char* name() const = 0;

    Callback<> timer_callback = {};
};

void add_clock_device(ClockDevice *d);
ClockDevice *get_best_clock_device();
