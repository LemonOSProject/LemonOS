#pragma once

template<typename ...Args>
struct Callback {
    void *data;
    void(*fn)(void *, Args...);

    template<typename T>
    static Callback create(T *cb) {  
        return {
            cb,
            [](void *data, Args... args) {
                (*(T*)data)(args...);
            }
        };
    }
};
