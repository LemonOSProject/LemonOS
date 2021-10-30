#include <PS2.h>

#include <APIC.h>
#include <CString.h>
#include <Device.h>
#include <Fs/Filesystem.h>
#include <IDT.h>
#include <Logging.h>
#include <System.h>
#include <stddef.h>

#include "PS2.h"

#define PACKET_QUEUE_SIZE 64

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

#define KEY_QUEUE_SIZE 256

uint8_t keyQueue[KEY_QUEUE_SIZE];

unsigned short keyQueueEnd = 0;
unsigned short keyQueueStart = 0;
unsigned short keyCount = 0;

template <bool isMouse> inline void WaitData() {
    int timeout = 10000;
    while (timeout--)
        if ((inportb(0x64) & 0x21) == (isMouse ? 0x21 : 0x1))
            return;
}

inline void WaitSignal() {
    int timeout = 10000;
    while (timeout--)
        if ((inportb(0x64) & 0x2) != 0x2)
            return;
}

template <bool isMouse> int SendCommand(uint8_t cmd) {
    uint8_t r = PS2_RESEND;
    int tries = 3;

    while (r == PS2_RESEND && tries--) {
        if constexpr (isMouse) {
            WaitSignal();
            outportb(0x64, 0xD4);
        }

        WaitSignal();
        outportb(0x60, cmd);

        WaitData<isMouse>();
        r = inportb(0x60);
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
    // Read from the keyboard's data buffer
    uint8_t key = inportb(0x60);

    if (keyCount >= KEY_QUEUE_SIZE)
        return; // Drop key

    // Add key to queue
    keyQueue[keyQueueEnd] = key;

    keyQueueEnd++;

    if (keyQueueEnd >= KEY_QUEUE_SIZE) {
        keyQueueEnd = 0;
    }

    keyCount++;
}

uint8_t mouseData[3];

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
void MouseHandler(void*, RegisterContext* regs) {
    switch (mouseCycle) {
    case 0:
        mouseData[0] = inportb(0x60);

        if (!(mouseData[0] & 0x8))
            break;

        mouseCycle++;
        break;
    case 1:
        mouseData[1] = inportb(0x60);
        mouseCycle++;
        break;
    case 2: {
        mouseData[2] = inportb(0x60);
        mouseCycle = 0;

        if (packetCount >= PACKET_QUEUE_SIZE)
            break; // Drop packet

        MousePacket pkt;
        pkt.buttons = mouseData[0] & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_MIDDLE | MOUSE_BUTTON_RIGHT);

        // Set sign bit accoridng to mouse packet
        int x = mouseData[1] - ((mouseData[0] & MOUSE_X_SIGN) << 4);
        int y = mouseData[2] - ((mouseData[1] & MOUSE_Y_SIGN) << 3);

        pkt.xMovement = x;
        pkt.yMovement = -y;

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

namespace PS2 {
KeyboardDevice kbDev("keyboard0");
MouseDevice mouseDev("mouse0");

void Initialize() {
    // Start by disabling both ports
    WaitSignal();
    outportb(0x64, 0xAD);
    WaitSignal();
    outportb(0x64, 0xA7);

    inportb(0x60); // Discard any data

    WaitSignal();
    outportb(0x64, 0x20);
    WaitData<false>();
    uint8_t status = inportb(0x20);

    if(status & PS2_STATUS_MOUSE_CLOCK) {
        // Must be dual channel
        WaitSignal();
        outportb(0x64, PS2_ENABLE_AUX_INPUT);
        int timeout = 1000;
        while (inportb(0x60) != 0x1 && timeout-- > 0)
            ;
        inportb(0x60);
    }

    outportb(0x64, 0xAE);
    //outportb(0x64, PS2_ENABLE_AUX_INPUT);

    status = ((inportb(0x60) & ~0x20) | 2); // Enable interrupts, enable mouse clock
    outportb(0x64, 0x60);
    WaitSignal();
    outportb(0x60, status);
    WaitData<false>();
    inportb(0x60); // ACK may have been sent

    SendCommand<true>(0xF6);
    SendCommand<true>(0xF4);

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
