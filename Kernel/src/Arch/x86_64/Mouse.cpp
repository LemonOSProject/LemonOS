#include <Mouse.h>

#include <APIC.h>
#include <Device.h>
#include <Fs/Filesystem.h>
#include <IDT.h>
#include <Logging.h>
#include <String.h>
#include <System.h>
#include <stddef.h>

#define PACKET_QUEUE_SIZE 64

namespace Mouse {
int8_t mouseData[3];

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

bool dataUpdated = false;

void Handler(void*, RegisterContext* regs) {
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

        pkt.xMovement = mouseData[1];
        pkt.yMovement = -mouseData[2];

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

inline void Wait(uint8_t type) {
    int timeout = 100000;
    if (type == 0) {
        while (timeout--) // Data
            if ((inportb(0x64) & 1) == 1)
                return;
    } else
        while (timeout--) // Signal
            if ((inportb(0x64) & 2) == 0)
                return;
}

inline void Write(uint8_t data) {
    Wait(1);
    outportb(0x64, 0xD4);
    Wait(1);
    // Send data
    outportb(0x60, data);
}

uint8_t Read() {
    // Get's response from mouse
    Wait(0);
    return inportb(0x60);
}

bool DataUpdated() {
    bool updated = dataUpdated;
    if (dataUpdated)
        dataUpdated = false;
    return updated;
}

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

MouseDevice mouseDev("mouse0");

void Install() {
    Wait(1);
    outportb(0x64, 0xA8); // Enable aux PS/2 input

    outportb(0x64, 0xFF); // Reset Mouse

    // Enable the interrupts
    Wait(1);
    outportb(0x64, 0x20);
    Wait(0);
    uint8_t status = ((inportb(0x60) & ~0x20) | 2); // Enable interrupts, enable mouse clock
    Wait(1);
    outportb(0x64, 0x60);
    Wait(1);
    outportb(0x60, status);

    Write(0xF6);
    Read();

    Write(0xF4);
    Read();

    IDT::RegisterInterruptHandler(IRQ0 + 12, Handler);
    APIC::IO::MapLegacyIRQ(12);
}

int8_t* GetData() { return mouseData; }
} // namespace Mouse