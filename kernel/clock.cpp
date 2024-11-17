#include "clock.h"

#include <le/lazy_constructed.h>
#include <le/list.h>

LazyConstructed<List<ClockDevice *>> devices;

void add_clock_device(ClockDevice *clock) {
    if (!devices.is_initialized)
        devices.construct();

    devices->push_back(clock);
}

ClockDevice *get_best_clock_device() {
    if (!devices.is_initialized || !devices->get_length())
        return nullptr;

    return devices->get_front();
}
