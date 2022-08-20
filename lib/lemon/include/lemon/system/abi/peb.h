#pragma once

#include <lemon/system/abi/types.h>

#include <stdint.h>

struct ProcessEnvironmentBlock {
    struct ProcessEnvironmentBlock* self;

    uint64_t executableBaseAddress;
    
    uint64_t hirakuBase;
    uint64_t hirakuSize;

    uint64_t sharedDataBase;
    uint64_t sharedDataSize;
};
