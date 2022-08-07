#include <SMP.h>

#include <ACPI.h>
#include <APIC.h>
#include <CPU.h>
#include <Device.h>
#include <HAL.h>
#include <IDT.h>
#include <Logging.h>
#include <Memory.h>
#include <TSS.h>
#include <Timer.h>

#include "smpdefines.inc"

static inline void wait(uint64_t ms) {
    Timer::Wait(ms);
}

extern void* _smp_trampoline_entry16;
extern void* _smp_trampoline_end;

volatile uint16_t* smpMagic = (uint16_t*)SMP_TRAMPOLINE_DATA_START_FLAG;
volatile uint16_t* smpID = (uint16_t*)SMP_TRAMPOLINE_CPU_ID;
gdt_ptr_t* smpGDT = (gdt_ptr_t*)SMP_TRAMPOLINE_GDT_PTR;
volatile uint64_t* smpCR3 = (uint64_t*)SMP_TRAMPOLINE_CR3;
volatile uint64_t* smpStack = (uint64_t*)SMP_TRAMPOLINE_STACK;
volatile uint64_t* smpEntry2 = (uint64_t*)SMP_TRAMPOLINE_ENTRY2;

volatile bool doneInit = false;

extern gdt_ptr_t GDT64Pointer64;
extern idt_ptr_t idtPtr;

void syscall_init();

namespace SMP {
CPU* cpus[256];
unsigned processorCount = 1;
tss_t tss1 __attribute__((aligned(16)));

void SMPEntry(uint16_t id) {
    CPU* cpu = cpus[id];
    cpu->currentThread = nullptr;
    cpu->runQueueLock = 0;
    SetCPULocal(cpu);

    cpu->gdt = Memory::KernelAllocate4KPages(
        1); // kmalloc(GDT64Pointer64.limit + 1); // Account for the 1 subtracted from limit
    Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)cpu->gdt, 1);
    memcpy(cpu->gdt, (void*)GDT64Pointer64.base, GDT64Pointer64.limit + 1); // Make a copy of the GDT
    cpu->gdtPtr = {.limit = GDT64Pointer64.limit, .base = (uint64_t)cpu->gdt};
    cpu->idtPtr = {.limit = idtPtr.limit, .base = idtPtr.base};

    asm volatile("lgdt (%%rax)" ::"a"(&cpu->gdtPtr));
    asm volatile("lidt %0" ::"m"(cpu->idtPtr));

    TSS::InitializeTSS(&cpu->tss, cpu->gdt);
    APIC::Local::Enable();

    cpu->runQueue = new FastList<Thread*>();

    doneInit = true;

    syscall_init();

    asm("sti");
    
    for (;;)
        asm volatile("pause");
}

void InitializeCPU(uint16_t id) {
    CPU* cpu = new CPU;
    cpu->id = id;
    cpu->runQueueLock = 0;
    cpus[id] = cpu;

    *smpMagic = 0;                                          // Set magic to 0
    *smpID = id;                                            // Set ID to our CPU's ID
    *smpEntry2 = (uint64_t)SMPEntry;                        // Our second entry point
    *smpStack = (uint64_t)kmalloc(16384);
    *smpStack += 16384;
    *smpGDT = GDT64Pointer64;

    asm volatile("mov %%cr3, %%rax" : "=a"(*smpCR3));

    APIC::Local::SendIPI(id, ICR_DSH_DEST, ICR_MESSAGE_TYPE_INIT, 0);
    wait(50);

    APIC::Local::SendIPI(id, ICR_DSH_DEST, ICR_MESSAGE_TYPE_STARTUP, (SMP_TRAMPOLINE_ENTRY >> 12));
    wait(50);

    if ((*smpMagic) != 0xB33F) { // Check if the trampoline code set the flag to let us know it has started
        wait(100);
    }

    if ((*smpMagic) != 0xB33F) {
        APIC::Local::SendIPI(id, ICR_DSH_DEST, ICR_MESSAGE_TYPE_STARTUP, (SMP_TRAMPOLINE_ENTRY >> 12));
        wait(200);
    }

    if ((*smpMagic) != 0xB33F) {
        Log::Error("[SMP] Failed to start CPU #%d, magic: %x", id, *smpMagic);
        return;
    }

    while (!doneInit)
        asm("pause");

    doneInit = false;
}

bool didInitializeCPU0 = false;
CPU cpu0;
void InitializeCPU0Context() {
    didInitializeCPU0 = true;
    cpus[0] = &cpu0;
    cpus[0]->id = 0;
    cpus[0]->gdt = (void*)GDT64Pointer64.base;
    cpus[0]->gdtPtr = GDT64Pointer64;
    cpus[0]->currentThread = nullptr;
    cpus[0]->runQueueLock = 0;
    SetCPULocal(cpus[0]);
}

void Initialize() {
    assert(didInitializeCPU0);
    // Initialize rest of CPU 0
    cpus[0]->runQueue = new FastList<Thread*>();

    if (HAL::disableSMP) {
        TSS::InitializeTSS(&cpus[0]->tss, cpus[0]->gdt);
        ACPI::processorCount = 1;
        processorCount = 1;
        return;
    }

    processorCount = ACPI::processorCount;

    memcpy((void*)SMP_TRAMPOLINE_ENTRY, &_smp_trampoline_entry16, ((uint64_t)&_smp_trampoline_end) - (uint64_t)(&_smp_trampoline_entry16));

    for (int i = 0; i < processorCount; i++) {
        if (ACPI::processors[i] != 0) {
            assert(!cpus[ACPI::processors[i]]);

            InitializeCPU(ACPI::processors[i]);
        }
    }

    TSS::InitializeTSS(&cpus[0]->tss, cpus[0]->gdt);
}
} // namespace SMP