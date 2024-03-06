#pragma once

#include <new>

template<typename T>
struct LazyConstructed {
    char data[sizeof(T)];
    bool is_initialized = false;

    inline T &operator*() {
        if (!is_initialized) {
            new (data) T();

            is_initialized = true;
        }

        return *(T *)data;
    }

    inline T *operator->() {
        if (!is_initialized) {
            new (data) T();

            is_initialized = true;
        }

        return (T *)data;
    }
};
