#pragma once

#include <assert.h>

#include <le/util.h>

template <typename T>
inline constexpr T round_up_power_2(const T val) {
    // Make sure we CAN actually round up
    // (there is enough space in the type)
    const_assert(static_cast<T>(1) << (sizeof(T) * 8 - 1) > val);

    T p = 1;
    while(p < val) {
        p <<= 1;
    }

    return p;
}

static_assert(round_up_power_2(3u) == 4);
static_assert(round_up_power_2(0xffffu) == 0x10000);
static_assert(round_up_power_2(0x8000u - 5) == 0x8000);
