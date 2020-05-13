#include <smp.h>

#include <cpu.h>
#include <devicemanager.h>
#include <apic.h>
#include <logging.h>
#include <timer.h>
#include <acpi.h>

#include "smpdefines.inc"

static inline void wait(uint64_t ms){
    uint64_t timeMs = Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * (1000.0 / Timer::GetFrequency()));
    while((Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * (1000.0 / Timer::GetFrequency()))) - timeMs <= ms); // Wait 20ms
}

extern void* _binary_smptrampoline_bin_start;
extern void* _binary_smptrampoline_bin_size;

uint16_t* smpMagic = (uint16_t*)SMP_TRAMPOLINE_DATA_MAGIC;
uint16_t* smpID = (uint16_t*)SMP_TRAMPOLINE_CPU_ID;
uint64_t* smpGDT = (uint64_t*)SMP_TRAMPOLINE_GDT_PTR;
uint64_t* smpCR3 = (uint64_t*)SMP_TRAMPOLINE_CR3;
uint64_t* smpStack = (uint64_t*)SMP_TRAMPOLINE_STACK;
uint64_t* smpEntry2 = (uint64_t*)SMP_TRAMPOLINE_ENTRY2;

namespace CPU{
    void SMPEntry(uint16_t id){
        Log::Info("[SMP] Hello from CPU #%d", id);

        // TODO: Give CPUs their own GDT (and TSS)

        for(;;);
    }

    void PrepareTrampoline(){
        memcpy((void*)SMP_TRAMPOLINE_ENTRY, &_binary_smptrampoline_bin_start, ((uint64_t)&_binary_smptrampoline_bin_size));
    }

    void InitializeCPU(uint16_t id){
        Log::Info("[SMP] Enabling CPU #%d", id);

        CPU* cpu = new CPU();

        cpu->id = id;

        *smpMagic = 0; // Set magic to 0
        *smpID = id; // Set ID to our CPU's ID
        *smpEntry2 = (uint64_t)SMPEntry; // Our second entry point
        *smpStack = (uint64_t)Memory::KernelAllocate4KPages(1); // 4K stack
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), *smpStack, 1);
        *smpStack += PAGE_SIZE_4K;

        asm volatile("sgdt (%0)" :: "r"(smpGDT));

        asm volatile("mov %%cr3, %%rax" : "=a"(*smpCR3));

        APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_INIT, 0);

        wait(20);

        if((*smpMagic) != 0xB33F){ // Check if the trampoline code set the flag to let us know it has started
            APIC::Local::SendIPI(id, ICR_MESSAGE_TYPE_STARTUP, (SMP_TRAMPOLINE_ENTRY >> 12));

            wait(200);
        }

        if((*smpMagic) != 0xB33F){
            Log::Error("[SMP] Failed to start CPU #%d", id);
        } else {
            Log::Info("[SMP] Successfully started CPU #%d ", id);
        }
    }

    void InitializeSMP(){
        PrepareTrampoline();

        for(int i = 0; i < ACPI::processorCount; i++){
            if(ACPI::processors[i] != 0)
                InitializeCPU(ACPI::processors[i]);
        }
    }
}