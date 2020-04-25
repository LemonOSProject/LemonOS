#pragma once

#include <stdint.h>

namespace Timer{

    uint64_t GetSystemUptime();
    uint32_t GetTicks();
    uint32_t GetFrequency();

    // Initialize
    void Initialize(uint32_t freq);
}