#pragma once

#include <Compiler.h>

namespace std {
    template<typename T>
    ALWAYS_INLINE constexpr T&& move(T&& t) noexcept {
        return static_cast<T&&>(t);
    }
}