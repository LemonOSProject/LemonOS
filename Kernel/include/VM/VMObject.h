#pragma once

#include <stdint.h>
#include <stddef.h>

class VMObject {
public:
    VMObject(uintptr_t base, size_t size);

    __attribute__(( always_inline )) inline uintptr_t Base() const { return base; }
    __attribute__(( always_inline )) inline uintptr_t Size() const { return size; }
private:
    uintptr_t base;
    size_t size;

    size_t refCount = 0; // References to the VM object

    bool anonymous : 1 = false;
    bool copyOnWrite : 1 = false;
};