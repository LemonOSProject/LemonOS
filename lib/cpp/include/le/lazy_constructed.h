#pragma once

#include <new>
#include <utility>

template <typename T> struct LazyConstructed {
    char data[sizeof(T)];
    bool is_initialized = false;

    template <typename... Args> inline void construct(Args &&...args) {
        if (!is_initialized) {
            new (data) T(std::move(args)...);

            is_initialized = true;
        }
    }

    inline T &operator*() {
        if (!is_initialized) {
            __builtin_unreachable();
        }

        return *(T *)data;
    }

    inline T *operator->() {
        if (!is_initialized) {
            __builtin_unreachable();
        }

        return (T *)data;
    }

    inline T *ptr() {
        return (T *)data;
    }
};
