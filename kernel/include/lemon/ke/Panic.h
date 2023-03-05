#pragma once

[[noreturn]] void KernelPanic(const char** reasons, int reasonCount);

template<typename... T>
[[noreturn]] void KernelPanic(T*... reasons){
    const char* list[] { reasons... };
    KernelPanic(list, sizeof...(reasons));
}