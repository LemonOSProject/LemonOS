#include <APIC.h>
#include <Device.h>
#include <Fs/Filesystem.h>
#include <IDT.h>
#include <Logging.h>
#include <Scheduler.h>
#include <System.h>

#define KEY_QUEUE_SIZE 256

namespace Keyboard {
uint8_t keyQueue[KEY_QUEUE_SIZE];

unsigned short keyQueueEnd = 0;
unsigned short keyQueueStart = 0;
unsigned short keyCount = 0;

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

KeyboardDevice kbDev("keyboard0");

// Interrupt handler
void Handler(void*, RegisterContext* r) {
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

// Register interrupt handler
void Install() {
    IDT::RegisterInterruptHandler(IRQ0 + 1, Handler);
    APIC::IO::MapLegacyIRQ(1);

    outportb(0xF0, 1); // Set scan code 1
}
} // namespace Keyboard
