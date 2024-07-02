#pragma once

#include <stdint.h>

namespace mm {

union MemoryProtection {
    struct {
        uint32_t read : 1;
        uint32_t write : 1;
        uint32_t execute : 1;
    };

    uint32_t prot = 0;

    static inline consteval MemoryProtection rw() {
        return {{true, true, false}};
    }

    static inline consteval MemoryProtection rx() {
        return {{true, false, true}};
    }

    static inline consteval MemoryProtection none() {
        return {{false, false, false}};
    }
};

} // namespace mm
