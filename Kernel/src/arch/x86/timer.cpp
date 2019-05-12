#include <stdint.h>
#include <idt.h>
#include <scheduler.h>

namespace Timer{
    uint32_t frequency; // Timer frequency
    uint32_t ticks; // Timer tick counter
    uint64_t uptime; // System uptime in seconds since the timer was initialized

    uint64_t GetSystemUptime(){
        return uptime;
    }

    uint32_t GetTicks(){
        return ticks;
    }

    // Timer handler
    void Handler(regs32_t *r) {
        ticks++;
        if(ticks >= frequency){
            uptime++;
            ticks -= frequency;
        }
        Scheduler::Tick();
    }

    // Initialize
    void Initialize(uint32_t freq) {
        frequency = freq;
        uint32_t divisor = 1193180 / freq;

        // Send the command byte.
        outportb(0x43, 0x36);

        // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
        uint8_t l = (uint8_t)(divisor & 0xFF);
        uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

        // Send the frequency divisor.
        outportb(0x40, l);
        outportb(0x40, h);

        IDT::RegisterInterruptHandler(IRQ0, Handler);
    }
}