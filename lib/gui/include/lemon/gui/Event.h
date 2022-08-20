#pragma once

#include <lemon/core/Util.h>
#include <lemon/graphics/Graphics.h>

#include <assert.h>

namespace Lemon {
enum Event {
    EventKeyPressed,
    EventKeyReleased,
    EventMousePressed,
    EventMouseReleased,
    EventRightMousePressed,
    EventRightMouseReleased,
    EventMiddleMousePressed,
    EventMiddleMouseReleased,
    EventMouseMoved,
    EventWindowClosed,
    EventWindowMinimized,
    EventWindowResize,
    EventWindowAdded,
    EventWindowRemoved,
    EventWindowCommand,
    EventMouseEnter,
    EventMouseExit,
};

struct LemonEvent {
    int32_t event;
    union {
        struct {
            int key;
            int keyModifiers;
        };
        vector2i_t mousePos;
        vector2i_t resizeBounds;
        unsigned short windowCmd;
        uint64_t bufferKey;
        uint64_t data;
    };
};

template<typename... Args>
struct EventHandler {
    void* data;
    void(*handler)(void*, Args...) = nullptr;

    template<typename T>
    void Set(void(*newHandler)(T*, Args...), T* newData = nullptr) {
        data = newData;
        handler = (void(*)(void*, Args...))newHandler;
    }

    void Set(std::nullptr_t) {
        handler = nullptr;
    }

    ALWAYS_INLINE void Run(Args... args) {
        if(handler) {
            handler(data, args...);
        }
    }

    ALWAYS_INLINE void operator()(Args... args) {
        return Run(args...);
    }
};

template<>
struct EventHandler<> {
    void* data;
    void(*handler)(void*) = nullptr;

    template<typename T>
    void Set(void(*newHandler)(T*), T* newData = nullptr) {
        data = newData;
        handler = (void(*)(void*))newHandler;
    }

    void Set(std::nullptr_t) {
        handler = nullptr;
    }

    ALWAYS_INLINE void Run() {
        if(handler) {
            handler(data);
        }
    }

    ALWAYS_INLINE void operator()() {
        return Run();
    }
};

} // namespace Lemon
