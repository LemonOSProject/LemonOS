#include <scheduler.h>
#include <system.h>
#include <idt.h>
#include <logging.h>
#include <gui.h>
#include <apic.h>

uint8_t key = 0;

namespace Keyboard{

    // Interrupt handler
    void Handler(regs64_t* r)
    {
        // Read from the keyboard's data buffer
        key = inportb(0x60);
		
		if(GetDesktop()) {
			message_t wmKeyMessage;
            wmKeyMessage.senderPID = 0;
            wmKeyMessage.recieverPID = GetDesktop()->pid; // Window Manager
            wmKeyMessage.msg = 0x1BEEF; // Desktop Event - Key Press
            wmKeyMessage.data = key; // The key that was pressed
			wmKeyMessage.data2 = 0;
			
            Scheduler::SendMessage(wmKeyMessage);
        }
    }

    // Register interrupt handler
    void Install() {
        IDT::RegisterInterruptHandler(IRQ0 + 1, Handler);
		APIC::IO::MapLegacyIRQ(1);
    }
}
