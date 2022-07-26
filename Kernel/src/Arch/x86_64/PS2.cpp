#include <PS2.h>

#include <ABI/Keyboard.h>

#include <APIC.h>
#include <CString.h>
#include <Device.h>
#include <Fs/Filesystem.h>
#include <IDT.h>
#include <IOPorts.h>
#include <Logging.h>
#include <stddef.h>

#include "PS2.h"

#define PACKET_QUEUE_SIZE 64

#define PS2_DATA 0x60
#define PS2_CMD 0x64

#define PS2_RESET 0xFF
#define PS2_ACK 0xFA
#define PS2_RESEND 0xFE
#define PS2_ENABLE_AUX_INPUT 0xA8

#define PS2_STATUS_MOUSE_CLOCK 0x20

#define MOUSE_BUTTON_LEFT 0
#define MOUSE_BUTTON_RIGHT 1
#define MOUSE_BUTTON_MIDDLE 2

#define MOUSE_X_SIGN (1 << 4)
#define MOUSE_Y_SIGN (1 << 5)
#define MOUSE_X_OVERFLOW (1 << 6)
#define MOUSE_Y_OVERFLOW (1 << 7)

#define KEY_QUEUE_SIZE 256

uint8_t keyQueue[KEY_QUEUE_SIZE];

extern const uint8_t scancode2Keymap[];
extern const uint8_t scancode2KeymapExtended[];

// Set true when a 0xe0 byte is received (indicating extended scancode)
bool keyIsExtended = false;
bool keyWasReleased = false;
unsigned short keyQueueEnd = 0;
unsigned short keyQueueStart = 0;
unsigned short keyCount = 0;

template <bool isMouse> inline void WaitData() {
    int timeout = 250;
    while (timeout--) {
        if constexpr(isMouse) {
            if ((inportb(PS2_CMD) & 0x21) == 0x20)
                return;
        } else {
            if (inportb(PS2_CMD) & 0x1)
                return;
        }

        Timer::Wait(1);
    }

    Log::Warning("[PS2] Timed out waiting for data");
}

inline void WaitSignal() {
    int timeout = 250;
    while (timeout--) {
        if ((inportb(PS2_CMD) & 0x2) != 0x2)
            return;
        Timer::Wait(1);
    }
}

template <bool isMouse> int SendCommand(uint8_t cmd) {
    uint8_t r = PS2_RESEND;
    int tries = 3;

    while (r == PS2_RESEND && tries--) {
        if constexpr (isMouse) {
            WaitSignal();
            outportb(PS2_CMD, 0xD4);
        }

        WaitSignal();
        outportb(PS2_DATA, cmd);

        WaitData<false>();
        r = inportb(PS2_DATA);
    }

    return r;
}

bool ReadKey(uint8_t* key) {
    if (keyCount <= 0)
        return false;

    *key = keyQueue[keyQueueStart];

    keyQueueStart++;

    if (keyQueueStart >= KEY_QUEUE_SIZE) {
        keyQueueStart = 0;
    }

    keyCount--;

    return true;
}

// Interrupt handler
void KBHandler(void*, RegisterContext* r) {
    if (!(inportb(PS2_CMD) & 1))
        return; // Wait for buffer

    // Read from the keyboard's data buffer
    uint8_t key = inportb(PS2_DATA);

    if(key == 0xe0) {
        keyIsExtended = true;
        return;
    } else if(key == 0xf0) {
        keyWasReleased = true;
        return;
    }

    int keyCode = 0;
    if(keyIsExtended) {
        keyCode = scancode2KeymapExtended[key];
    }
    
    // If not an extended key OR
    // the corresponding extended entry is 0,
    // use the normal keymap
    if(keyCode == 0) {
        keyCode = scancode2Keymap[key];
    }

    if(keyWasReleased) {
        // Set high bit if key was released
        keyCode |= 0x80;
    }
    
    keyIsExtended = false;
    keyWasReleased = false;

    if (keyCount >= KEY_QUEUE_SIZE)
        return; // Drop key

    // Add key to queue
    keyQueue[keyQueueEnd] = keyCode;

    keyQueueEnd++;

    if (keyQueueEnd >= KEY_QUEUE_SIZE) {
        keyQueueEnd = 0;
    }

    keyCount++;
}

uint8_t mouseData[4];

struct MousePacket {
    int8_t buttons;
    int8_t xMovement;
    int8_t yMovement;
    int8_t verticalScroll;
};

MousePacket packetQueue[PACKET_QUEUE_SIZE]; // Use a statically allocated array to avoid allocations

short packetQueueEnd = 0;
short packetQueueStart = 0;
short packetCount = 0;

uint8_t mouseCycle = 0;
bool hasScrollWheel = false;
void MouseHandler(void*, RegisterContext* regs) {
    switch (mouseCycle) {
    case 0:
        mouseData[0] = inportb(PS2_DATA);

        if (!(mouseData[0] & 0x8))
            break;

        mouseCycle++;
        break;
    case 1:
        mouseData[1] = inportb(PS2_DATA);
        mouseCycle++;
        break;
    case 2:
        mouseData[2] = inportb(PS2_DATA);

        if (hasScrollWheel) {
            mouseCycle++;
            break;
        }
    case 3: {
        if (hasScrollWheel) {
            mouseData[3] = inportb(PS2_DATA);
        }
        mouseCycle = 0;

        if (packetCount >= PACKET_QUEUE_SIZE)
            break; // Drop packet

        MousePacket pkt;
        pkt.buttons = mouseData[0] & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_MIDDLE | MOUSE_BUTTON_RIGHT);

        // Set sign bit accoridng to mouse packet
        int x = mouseData[1] - ((mouseData[0] & MOUSE_X_SIGN) << 4);
        int y = mouseData[2] - ((mouseData[1] & MOUSE_Y_SIGN) << 3);

        if (mouseData[0] & (MOUSE_X_OVERFLOW | MOUSE_Y_OVERFLOW)) {
            // If either overflow bit is set, discard movement
            x = 0;
            y = 0;
        }

        pkt.xMovement = x;
        pkt.yMovement = -y;
        pkt.verticalScroll = mouseData[3];

        // Add packet to queue
        packetQueue[packetQueueEnd] = pkt;

        packetQueueEnd++;

        if (packetQueueEnd >= PACKET_QUEUE_SIZE) {
            packetQueueEnd = 0;
        }

        packetCount++;
        break;
    }

    default: {
        mouseCycle = 0;
        break;
    }
    }
}

class KeyboardDevice : public Device {
public:
    DirectoryEntry dirent;

    KeyboardDevice(char* name) : Device(name, DeviceTypeLegacyHID) {
        flags = FS_NODE_CHARDEVICE;
        strcpy(dirent.name, name);
        dirent.flags = flags;
        dirent.node = this;

        SetDeviceName("PS/2 Keyboard Device");
    }

    ssize_t Read(size_t offset, size_t size, uint8_t* buffer) {
        if (size > keyCount)
            size = keyCount;

        if (!size)
            return 0;

        unsigned short i = 0;
        for (; i < size; i++) {
            if (!ReadKey(buffer++)) // Insert key and increment
                break;
        }

        return i;
    }
};

class MouseDevice : public Device {
public:
    DirectoryEntry dirent;

    MouseDevice(char* name) : Device(name, DeviceTypeLegacyHID) {
        flags = FS_NODE_CHARDEVICE;
        strcpy(dirent.name, name);
        dirent.flags = flags;
        dirent.node = this;

        SetDeviceName("PS/2 Mouse Device");
    }

    ssize_t Read(size_t offset, size_t size, uint8_t* buffer) {
        if (size < sizeof(MousePacket))
            return 0;

        if (packetCount <= 0)
            return 0; // No packets

        MousePacket* pkt = (MousePacket*)buffer;
        *pkt = packetQueue[packetQueueStart];

        packetQueueStart++;

        if (packetQueueStart >= PACKET_QUEUE_SIZE) {
            packetQueueStart = 0;
        }

        packetCount--;

        return sizeof(MousePacket);
    }
};

// Some touchpads want 'sliced commands'
// They are encoded in resolution commands with a leading
// scale command
void SendSlicedMouseCommand(uint8_t cmd) {
    SendCommand<true>(0xE6);

    for (int i = 3; i >= 0; i--) {
        SendCommand<true>(0xE8);
        SendCommand<true>(cmd >> (i * 2));
    }
}

namespace PS2 {
KeyboardDevice kbDev("keyboard0");
MouseDevice mouseDev("mouse0");

void Initialize() {
    // Start by disabling both ports
    WaitSignal();
    outportb(PS2_CMD, 0xAD);
    WaitSignal();
    outportb(PS2_CMD, 0xA7);

    while (inportb(PS2_CMD) & 1) {
        inportb(PS2_DATA); // Discard any data
    }

    WaitSignal();
    outportb(PS2_CMD, 0x20);
    WaitData<false>();
    uint8_t status = inportb(0x20);

    // Enable interrupts, enable keyboard and mouse clock
    // disable scancode translation
    status = ((status & ~0x70) | 3);
    WaitSignal();
    outportb(PS2_CMD, 0x60);
    WaitSignal();
    outportb(PS2_DATA, status);

    Timer::Wait(50);

    // Renable both ports
    WaitSignal();
    outportb(PS2_CMD, 0xAE);
    WaitSignal();
    outportb(PS2_CMD, PS2_ENABLE_AUX_INPUT);

    // Set keyboard scancode set 2
    SendCommand<false>(0xF0);
    SendCommand<false>(2);

    SendCommand<true>(0xF6);
    // Enable mouse packets
    SendCommand<true>(0xF4);

    // Set the sample rate a bunch of times
    // to enable scroll wheel
    SendCommand<true>(0xF3);
    SendCommand<true>(200);
    SendCommand<true>(0xF3);
    SendCommand<true>(100);
    SendCommand<true>(0xF3);
    SendCommand<true>(80);
    // Get MouseID
    SendCommand<true>(0xF2);
    WaitData<true>();
    int id = inportb(PS2_DATA);
    Log::Info("[PS/2] MouseID: %d", id);
    if (id >= 3) {
        hasScrollWheel = true;
    }

    // Attempt to reset synaptics touchpads
    /*SendSlicedMouseCommand(0);
    SendCommand<true>(0xE8);
    SendCommand<true>(0x28);
    WaitData<true>();*/

    // Linear scaling
    SendCommand<true>(0xE6);
    // 2 px/mm
    SendCommand<true>(0xE8);
    SendCommand<true>(1);

    IDT::RegisterInterruptHandler(IRQ0 + 12, MouseHandler);
    APIC::IO::MapLegacyIRQ(12);

    IDT::RegisterInterruptHandler(IRQ0 + 1, KBHandler);
    APIC::IO::MapLegacyIRQ(1);
}
} // namespace PS2

// clang-format off
constexpr uint8_t scancode2Keymap[256] = {
    0,
    KEY_F9,
    0,
    KEY_F5,
    KEY_F3,
    KEY_F1,
    KEY_F2,
    KEY_F12,
    0,
    KEY_F10,
    KEY_F8,
    KEY_F6,
    KEY_F4,
    KEY_TAB,
    KEY_BACKTICK,
    0,
    0,
    KEY_LALT,
    KEY_LSHIFT,
    0,
    KEY_LCTRL,
    KEY_Q,
    KEY_1,
    0,
    0,
    0,
    KEY_Z,
    KEY_S,
    KEY_A,
    KEY_W,
    KEY_2,
    0,
    0,
    KEY_C,
    KEY_X,
    KEY_D,
    KEY_E,
    KEY_4,
    KEY_3,
    0,
    0,
    KEY_SPACE,
    KEY_V,
    KEY_F,
    KEY_T,
    KEY_R,
    KEY_5,
    0,
    0,
    KEY_N,
    KEY_B,
    KEY_H,
    KEY_G,
    KEY_Y,
    KEY_6,
    0,
    0,
    0,
    KEY_M,
    KEY_J,
    KEY_U,
    KEY_7,
    KEY_8,
    0,
    0,
    KEY_COMMA,
    KEY_K,
    KEY_I,
    KEY_O,
    KEY_0,
    KEY_9,
    0,
    0,
    KEY_DOT,
    KEY_SLASH,
    KEY_L,
    KEY_SEMICOLON,
    KEY_P,
    KEY_MINUS,
    0,
    0,
    0,
    KEY_APOSTROPHE,
    0,
    KEY_LBRACE,
    KEY_EQUAL,
    0,
    0,
    KEY_CAPS,
    KEY_RSHIFT,
    KEY_ENTER,
    KEY_RBRACE,
    0,
    KEY_BACKSLASH,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    KEY_BACKSPACE,
    0,
    0,
    KEY_KP_1,
    0,
    KEY_KP_4,
    KEY_KP_7,
    0,
    0,
    0,
    KEY_KP_0,
    KEY_KP_DOT,
    KEY_KP_2,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_8,
    KEY_ESC,
    KEY_NUMLOCK,
    KEY_F11,
    KEY_KP_PLUS,
    KEY_KP_3,
    KEY_KP_MINUS,
    KEY_KP_ASTERISK,
    KEY_KP_9,
    KEY_SCLOCK,
    0,
    0,
    0,
    0,
    KEY_F7
};

constexpr uint8_t scancode2KeymapExtended[256] = {
    0,
    KEY_RALT,
    0,
    0,
    KEY_RCTRL,
    0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0,
    KEY_GUI
};
// clang-format on
