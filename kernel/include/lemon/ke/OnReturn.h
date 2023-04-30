#pragma once

#include <Compiler.h>

template <typename T>
concept CleanupFunction = requires(T t) {
    t();
};

template <CleanupFunction F> class OnReturn {
public:
    ALWAYS_INLINE OnReturn(F&& func) : m_func(func) {}

    ALWAYS_INLINE ~OnReturn() { m_func(); }

private:
    F m_func;
};
