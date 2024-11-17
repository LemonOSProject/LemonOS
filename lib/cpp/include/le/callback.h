#pragma once

#include <utility>

template<typename ...Args>
struct Callback {
    void *data;
    void(*fn)(void *, Args...);

    operator bool() const {
        return fn;
    }

    inline void call(Args... args) {
        fn(data, std::move(args)...);
    }

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
