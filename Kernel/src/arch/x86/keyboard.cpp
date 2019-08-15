#include <scheduler.h>
#include <system.h>
#include <idt.h>
#include <logging.h>

uint8_t key = 0;
bool caps = false;

char keymap_us[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	0,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   0,		/* Left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
	0,	/* Alt */
	' ',	/* Space bar */
	0,	/* Caps lock */
	0,	/* 59 - F1 key ... > */
	0,   0,   0,   0,   0,   0,   0,   0,
	0,	/* < ... F10 */
	0,	/* 69 - Num lock*/
	0,	/* Scroll Lock */
	0,	/* Home key */
	0,	/* Up Arrow */
	0,	/* Page Up */
	'-',
	0,	/* Left Arrow */
	0,
	0,	/* Right Arrow */
	'+',
	0,	/* 79 - End key*/
	0,	/* Down Arrow */
	0,	/* Page Down */
	0,	/* Insert Key */
	0,	/* Delete Key */
	0,   0,   0,
	0,	/* F11 Key */
	0,	/* F12 Key */
	0,	/* All other keys are undefined */
};

namespace Keyboard{
    message_t wmKeyMessage;

    char test[2];

    // Interrupt handler
    void Handler(regs32_t* r)
    {
        asm("cli");
        // Read from the keyboard's data buffer
        key = inportb(0x60);
		
		if(key == 0x3A){
            caps = !caps;
        } else {
            test[0] = keymap_us[(key >> 7) ? key-128 :key];
            test[1] = 0;
            Log::Write(test);
            wmKeyMessage.senderPID = 0;
            wmKeyMessage.recieverPID = 1; // Should be the Window Manager
            wmKeyMessage.id = 0x1BEEF; // Desktop Event - Key Press
            wmKeyMessage.data = key; // The key that was pressed
            Scheduler::SendMessage(wmKeyMessage);
        }

        asm("sti");
    }

    // Register interrupt handler
    void Install() {
        IDT::RegisterInterruptHandler(33, Handler);

		//outportb(0x64, )

        wmKeyMessage.senderPID = 0;
        wmKeyMessage.recieverPID = 1; // Should be the Window Manager
        wmKeyMessage.id = 0x1BEEF; // Desktop Event - Key Press
    }
}