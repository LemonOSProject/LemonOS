#include <scheduler.h>
#include <system.h>
#include <idt.h>
#include <logging.h>
#include <gui.h>
#include <apic.h>
#include <fs/filesystem.h>

#define KEY_QUEUE_SIZE 256

namespace Keyboard{
    uint8_t keyQueue[KEY_QUEUE_SIZE];

	unsigned short keyQueueEnd = 0;
	unsigned short keyQueueStart = 0;
	unsigned short keyCount = 0;

    bool ReadKey(uint8_t* key){
        if(keyCount <= 0) return false;

        *key = keyQueue[keyQueueStart];

        keyQueueStart++;

        if(keyQueueStart >= KEY_QUEUE_SIZE) {
            keyQueueStart = 0;
        }

        keyCount--;

        return true;
    }

	class KeyboardDevice : public FsNode{
	public:
        DirectoryEntry dirent;

		KeyboardDevice(char* name){
            flags = FS_NODE_CHARDEVICE;
            strcpy(dirent.name, name);
            dirent.flags = flags;
            dirent.node = this;
		}

		ssize_t Read(size_t offset, size_t size, uint8_t *buffer){
            if(size > keyCount) size = keyCount;

            if(!size) return 0;

            for(unsigned short i = 0; i < size; i++){
                ReadKey(buffer++); // Insert key and increment
            }

            return size;
		}
	};

    KeyboardDevice kbDev("keyboard0");

    // Interrupt handler
    void Handler(regs64_t* r)
    {
        // Read from the keyboard's data buffer
        uint8_t key = inportb(0x60);
		
        if(keyCount >= KEY_QUEUE_SIZE) return; // Drop key

        // Add key to queue
        keyQueue[keyQueueEnd] = key;

        keyQueueEnd++;
        
        if(keyQueueEnd >= KEY_QUEUE_SIZE) {
            keyQueueEnd = 0;
        }

        keyCount++;
    }

    // Register interrupt handler
    void Install() {
        fs::RegisterDevice(&kbDev.dirent);

        IDT::RegisterInterruptHandler(IRQ0 + 1, Handler);
		APIC::IO::MapLegacyIRQ(1);
    }
}
