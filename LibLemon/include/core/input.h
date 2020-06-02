
#pragma once

#include <stdint.h>
#include <unistd.h>

namespace Lemon{
    enum MouseButton{
        Left = 0x1,
        Right = 0x2,
        Middle = 0x4,
    };

    struct MousePacket{
        int8_t buttons;
        int8_t xMovement;
        int8_t yMovement;
        int8_t verticalScroll;
    };

    int PollMouse(MousePacket& pkt);
    ssize_t PollKeyboard(uint8_t* buffer, size_t count);
}