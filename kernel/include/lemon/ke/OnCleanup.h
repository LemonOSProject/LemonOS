#pragma once

#include <Compiler.h>

template <typename T>
concept CleanupFunction = requires(T t) {
    t();
};

template <CleanupFunction F> class OnCleanup {
public:
    ALWAYS_INLINE OnCleanup(F&& func) : m_func(func) {}

    ALWAYS_INLINE ~OnCleanup() { m_func(); }

private:
    F m_func;
};
