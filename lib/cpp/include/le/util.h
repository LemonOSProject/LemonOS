#pragma once

#include <assert.h>

template<typename T>
constexpr T min(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T>
constexpr T pow(T n, T exp) {
    while (exp--) {
        n *= n;
    }

    return n;
}

constexpr bool is_constant_eval() {
    if consteval { return true; } else { return false; }
}

template<typename T>
constexpr void const_assert(T expr) {
    if consteval {
        expr ? (void)0 : __builtin_unreachable();
    } else {
        assert(expr);
    }
}
