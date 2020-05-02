#pragma once

#include <device.h>
#include <stdint.h>

namespace CPU{
    class CPU : public Device{
        public:
        uint64_t id; // APIC/CPU id

    };
}