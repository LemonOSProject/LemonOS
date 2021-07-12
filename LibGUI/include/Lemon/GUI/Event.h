#pragma once

#include <Lemon/Graphics/Graphics.h>

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
        int key;
        vector2i_t mousePos;
        vector2i_t resizeBounds;
        unsigned short windowCmd;
        uint64_t bufferKey;
        uint64_t data;
    };
};
} // namespace Lemon
