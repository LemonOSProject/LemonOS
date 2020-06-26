#pragma once

#include <core/message.h>
#include <gfx/graphics.h>

namespace Lemon {
    enum Event{
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
    };

    typedef struct LemonEvent {
        uint32_t event;
        uint16_t length;
        union {
            int key;
            vector2i_t mousePos;
            vector2i_t resizeBounds;
            unsigned short windowCmd;
        };
    } __attribute__((packed)) lemon_event_t;
}