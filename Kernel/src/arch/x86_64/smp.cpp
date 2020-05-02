#include <cpu.h>

#include <devicemanager.h>
#include <apic.h>
#include <logging.h>

namespace CPU{
    void InitializeCPU(uint64_t id){
        Log::Info("[SMP] Enabling CPU #%d", id);

        CPU* cpu = new CPU();

        cpu->id = id;

        asm("cli");

        APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_INIT, 0);

        for(;;);

        APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_STARTUP, 0);

        asm("hlt");

        DeviceManager::devices->add_back(cpu);
    }
}