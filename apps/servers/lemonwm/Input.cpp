#include "Input.h"

#include "WM.h"

#include <lemon/core/Input.h>
#include <lemon/core/Keyboard.h>

#include <algorithm>

extern int usKeymap[];
extern int usKeymapShift[];

InputManager::InputManager(const Vector2i& mouseBounds) : m_mouseBounds(mouseBounds) {}

void InputManager::Poll() {
    auto handlePacketPress = [=](Lemon::MousePacket& pkt) -> void {
        if ((!!(pkt.buttons & Lemon::MouseButton::Left)) !=
            mouse.left) { // Use a double negative to make the statement 0 or 1
            mouse.left = !!(pkt.buttons & Lemon::MouseButton::Left);

            if (mouse.left) {
                WM::Instance().OnMouseDown(false);
            } else {
                WM::Instance().OnMouseUp(false);
            }
        }

        if ((!!(pkt.buttons & Lemon::MouseButton::Right)) !=
            mouse.right) { // Use a double negative to make the statement 0 or 1
            mouse.right = !!(pkt.buttons & Lemon::MouseButton::Right);

            if (mouse.right) {
                WM::Instance().OnMouseDown(true);
            } else {
                WM::Instance().OnMouseUp(true);
            }
        }
    };

    if (Lemon::MousePacket mousePacket; Lemon::PollMouse(mousePacket)) {
        mouse.pos.x = std::max(0, std::min(mouse.pos.x + mousePacket.xMovement, m_mouseBounds.x));
        mouse.pos.y = std::max(0, std::min(mouse.pos.y + mousePacket.yMovement, m_mouseBounds.y));

        handlePacketPress(mousePacket);

        while (Lemon::PollMouse(mousePacket)) {
            mouse.pos.x = std::max(0, std::min(mouse.pos.x + mousePacket.xMovement, m_mouseBounds.x));
            mouse.pos.y = std::max(0, std::min(mouse.pos.y + mousePacket.yMovement, m_mouseBounds.y));

            handlePacketPress(mousePacket); // If necessary send a mouse press event for each packet, however only send
                                            // move event after processing all packets
        }

        WM::Instance().OnMouseMove();
    }

    uint8_t buf[32];
    ssize_t count = Lemon::PollKeyboard(buf, 32);

    for (ssize_t i = 0; i < count; i++) {
        uint8_t code = buf[i] & 0x7F;
        bool isPressed = !((buf[i] >> 7) & 1);
        int key = 0;

        if (keyboard.shift) {
            key = usKeymapShift[code];
        } else {
            key = usKeymap[code];
        }

        switch (key) {
        case KEY_SHIFT:
            keyboard.shift = isPressed;
            break;
        case KEY_CONTROL:
            keyboard.control = isPressed;
            break;
        case KEY_ALT:
            keyboard.alt = isPressed;
            break;
        case KEY_CAPS:
            if (isPressed)
                keyboard.caps = !keyboard.caps;
            break;
        }

        WM::Instance().OnKeyUpdate(key, isPressed);
    }
}


int usKeymap[128] = {
    0,
    27,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8', // 9
    '9',
    '0',
    '-',
    '=',
    '\b', // Backspace
    '\t', // Tab
    'q',
    'w',
    'e',
    'r', // 19
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',        // Enter key
    KEY_CONTROL, // 29 - Control
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';', // 39
    '\'',
    '`',
    KEY_SHIFT, // Left shift
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n', // 49
    'm',
    ',',
    '.',
    '/',
    KEY_SHIFT, // Right shift
    '*',
    KEY_ALT,  // Alt
    ' ',      // Space bar
    KEY_CAPS, // Caps lock
    KEY_F1,   /* 59 - F1 key ... > */
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,      /* < ... F10 */
    0,            /* 69 - Num lock*/
    0,            // Scroll Lock
    KEY_HOME,     // Home key
    KEY_ARROW_UP, // Up Arrow
    0,            // Page Up
    '-',
    KEY_ARROW_LEFT, // Left Arrow
    0,
    KEY_ARROW_RIGHT, // Right Arrow
    '+',
    KEY_END,        /* 79 - End key*/
    KEY_ARROW_DOWN, // Down Arrow
    0,              // Page Down
    0,              // Insert Key
    KEY_DELETE,     // Delete Key
    0,
    0,
    0,
    0, // F11 Key
    0, // F12 Key
    0, // All other keys are undefined
    0,
    0,
    0,       // 90
    KEY_GUI, // Left GUI key
    KEY_GUI, // Right GUI key
};

int usKeymapShift[128] = {
    0,
    27,
    '!',
    '@',
    '#',
    '$',
    '%',
    '^',
    '&',
    '*', // 9
    '(',
    ')',
    '_',
    '+',
    '\b', // Backspace
    '\t', // Tab
    'Q',
    'W',
    'E',
    'R', // 19
    'T',
    'Y',
    'U',
    'I',
    'O',
    'P',
    '{',
    '}',
    '\n',        // Enter key
    KEY_CONTROL, // 29 - control
    'A',
    'S',
    'D',
    'F',
    'G',
    'H',
    'J',
    'K',
    'L',
    ':', // 39
    '\"',
    '~',
    KEY_SHIFT, // Left shift
    '|',
    'Z',
    'X',
    'C',
    'V',
    'B',
    'N', // 49
    'M',
    '<',
    '>',
    '?',
    KEY_SHIFT, // Right shift
    '*',
    KEY_ALT,  // Alt
    ' ',      // Space bar
    KEY_CAPS, // Caps lock
    KEY_F1,   /* 59 - F1 key ... > */
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,      /* < ... F10 */
    0,            /* 69 - Num lock*/
    0,            // Scroll Lock
    0,            // Home key
    KEY_ARROW_UP, // Up Arrow
    0,            // Page Up
    '-',
    KEY_ARROW_LEFT, // Left Arrow
    0,
    KEY_ARROW_RIGHT, // Right Arrow
    '+',
    0,              /* 79 - End key*/
    KEY_ARROW_DOWN, // Down Arrow
    0,              // Page Down
    0,              // Insert Key
    KEY_DELETE,     // Delete Key
    0,
    0,
    0,
    0, // F11 Key
    0, // F12 Key
    0, // All other keys are undefined
    0,
    0,
    0,       // 90
    KEY_GUI, // Left GUI key
    KEY_GUI, // Right GUI key
};