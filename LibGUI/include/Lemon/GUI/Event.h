#pragma once

#include <Lemon/Core/Util.h>
#include <Lemon/Graphics/Graphics.h>

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

struct EventHandler {
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
        assert(handler);
        handler(data);
    }

    ALWAYS_INLINE void operator()() {
        return Run();
    }
};

} // namespace Lemon
