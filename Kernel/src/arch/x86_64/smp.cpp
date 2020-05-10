#include <cpu.h>

#include <devicemanager.h>
#include <apic.h>
#include <logging.h>
#include <timer.h>

static inline void wait(uint64_t ms){
    uint64_t timeMs = Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * (1000.0 / Timer::GetFrequency()));
    while((Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * (1000.0 / Timer::GetFrequency()))) - timeMs <= ms); // Wait 20ms
}

namespace CPU{
    void InitializeCPU(uint64_t id){
        return;

        Log::Info("[SMP] Enabling CPU #%d", id);

        CPU* cpu = new CPU();

        cpu->id = id;

        APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_INIT, 0);

        wait(20);

        APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_STARTUP, 1);

        wait(200);

        APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_STARTUP, 1);
    }
}