#include <smp.h>

#include <cpu.h>
#include <devicemanager.h>
#include <apic.h>
#include <logging.h>
#include <timer.h>
#include <acpi.h>
#include <tss.h>
#include <idt.h>

#include "smpdefines.inc"

static inline void wait(uint64_t ms){
    uint64_t timeMs = Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * (1000.0 / Timer::GetFrequency()));
    while((Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * (1000.0 / Timer::GetFrequency()))) - timeMs <= ms); // Wait 20ms
}

extern void* _binary_smptrampoline_bin_start;
extern void* _binary_smptrampoline_bin_size;

uint16_t* smpMagic = (uint16_t*)SMP_TRAMPOLINE_DATA_MAGIC;
uint16_t* smpID = (uint16_t*)SMP_TRAMPOLINE_CPU_ID;
gdt_ptr_t* smpGDT = (gdt_ptr_t*)SMP_TRAMPOLINE_GDT_PTR;
uint64_t* smpCR3 = (uint64_t*)SMP_TRAMPOLINE_CR3;
uint64_t* smpStack = (uint64_t*)SMP_TRAMPOLINE_STACK;
uint64_t* smpEntry2 = (uint64_t*)SMP_TRAMPOLINE_ENTRY2;

extern gdt_ptr_t GDT64Pointer64;

namespace SMP{
    CPU* cpus[256];
    tss_t tss1 __attribute__((aligned(16)));

    void SMPEntry(uint16_t id){
        Log::Info("[SMP] Hello from CPU #%d", id);

        CPU* cpu = cpus[id];
        cpu->currentProcess = nullptr;
        SetCPULocal(cpu);
        
        cpu->gdt = Memory::KernelAllocate4KPages(1);//kmalloc(GDT64Pointer64.limit + 1); // Account for the 1 subtracted from limit
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)cpu->gdt, 1);
        memcpy(cpu->gdt, (void*)GDT64Pointer64.base, GDT64Pointer64.limit + 1); // Make a copy of the GDT
        cpu->gdtPtr = {.limit = GDT64Pointer64.limit, .base = (uint64_t)cpu->gdt};

        asm volatile("lgdt (%%rax)" :: "a"(&cpu->gdtPtr));

        idt_flush();

        TSS::InitializeTSS(&cpu->tss, cpu->gdt);

        APIC::Local::Enable();

        asm("sti");

        for(;;);
    }

    void PrepareTrampoline(){
        memcpy((void*)SMP_TRAMPOLINE_ENTRY, &_binary_smptrampoline_bin_start, ((uint64_t)&_binary_smptrampoline_bin_size));
    }

    void InitializeCPU(uint16_t id){
        Log::Info("[SMP] Enabling CPU #%d", id);

        CPU* cpu = new CPU;
        cpu->id = id;
        cpus[id] = cpu;
        
        *smpMagic = 0; // Set magic to 0
        *smpID = id; // Set ID to our CPU's ID
        *smpEntry2 = (uint64_t)SMPEntry; // Our second entry point
        *smpStack = (uint64_t)Memory::KernelAllocate4KPages(1); // 4K stack
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), *smpStack, 1);
        *smpStack += PAGE_SIZE_4K;
        *smpGDT = GDT64Pointer64;

        asm volatile("mov %%cr3, %%rax" : "=a"(*smpCR3));

        APIC::Local::SendIPI(id, ICR_DSH_DEST, ICR_MESSAGE_TYPE_INIT, 0);

        wait(20);

        if((*smpMagic) != 0xB33F){ // Check if the trampoline code set the flag to let us know it has started
            APIC::Local::SendIPI(id, ICR_DSH_DEST, ICR_MESSAGE_TYPE_STARTUP, (SMP_TRAMPOLINE_ENTRY >> 12));

            wait(200);
        }

        if((*smpMagic) != 0xB33F){
            Log::Error("[SMP] Failed to start CPU #%d", id);
        } else {
            Log::Info("[SMP] Successfully started CPU #%d ", id);
        }
    }

    void Initialize(){

        cpus[0] = new CPU; // Initialize CPU 0
        cpus[0]->id = 0;
        cpus[0]->gdt = (void*)GDT64Pointer64.base;
        cpus[0]->gdtPtr = GDT64Pointer64;
        cpus[0]->currentProcess = nullptr;
        SetCPULocal(cpus[0]);

        PrepareTrampoline();

        for(int i = 0; i < ACPI::processorCount; i++){
            if(ACPI::processors[i] != 0)
                InitializeCPU(ACPI::processors[i]);
        }
        
        TSS::InitializeTSS(&cpus[0]->tss, cpus[0]->gdt);
    }
}