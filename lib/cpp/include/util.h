#pragma once

template<typename T>
constexpr T min(T a, T b) {
    return (a < b) ? a : b;
}
