#pragma once

#include <String.h>

#include <stdint.h>

struct HirakuSymbol {
    uintptr_t address;
    const char* name;
};

void LoadHirakuSymbols();
HirakuSymbol* ResolveHirakuSymbol(const char* name);
