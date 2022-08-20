#include <APIC.h>

#include <ACPI.h>
#include <CPU.h>
#include <IDT.h>
#include <Logging.h>
#include <Paging.h>
#include <SMP.h>

#include <Debug.h>

#define APIC_READ(off) *((volatile uint32_t*)(virtualBase + off))
#define APIC_WRITE(off, val) (*((volatile uint32_t*)(virtualBase + off)) = val)

volatile uintptr_t virtualBase;

namespace APIC {
namespace Local {
uintptr_t base;

void SpuriousInterruptHandler(void*, RegisterContext* r) { Log::Warning("[APIC] Spurious Interrupt"); }

uint64_t ReadBase() {
    uint64_t low;
    uint64_t high;
    asm("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1B));

    return (high << 32) | low;
}

void WriteBase(uint64_t val) {
    uint64_t low = val & 0xFFFFFFFF;
    uint64_t high = val >> 32;
    asm("wrmsr" ::"a"(low), "d"(high), "c"(0x1B));
}

void Enable() {
    WriteBase(ReadBase() | (1UL << 11));

    APIC_WRITE(LOCAL_APIC_SIVR, APIC_READ(LOCAL_APIC_SIVR) | 0x1FF /* Enable APIC, Vector 255*/);
}

int Initialize() {
    Local::base = ReadBase() & LOCAL_APIC_BASE;
    virtualBase = Memory::GetIOMapping(base);

    if (debugLevelInterrupts >= DebugLevelNormal) {
        Log::Info("[APIC] Local APIC Base %x (%x)", base, virtualBase);
    }

    IDT::RegisterInterruptHandler(0xFF, SpuriousInterruptHandler);

    Enable();

    return 0;
}

void SendIPI(uint8_t destination, uint32_t dsh /* Destination Shorthand*/, uint32_t type, uint8_t vector) {
    uint32_t high = ((uint32_t)destination) << 24;
    uint32_t low = dsh | type | ICR_VECTOR(vector);

    APIC_WRITE(LOCAL_APIC_ICR_HIGH, high);
    APIC_WRITE(LOCAL_APIC_ICR_LOW, low);
}
} // namespace Local

namespace IO {
uintptr_t base = 0;
uintptr_t virtualBase;
volatile uint32_t* registerSelect;
volatile uint32_t* ioWindow;

uint32_t interrupts;
uint32_t apicID;

uint32_t Read32(uint32_t reg) {
    *registerSelect = reg;
    return *ioWindow;
}

void Write32(uint32_t reg, uint32_t data) {
    *registerSelect = reg;
    *ioWindow = data;
}

uint64_t Read64(uint32_t reg) {
    assert(!"unimplemented");
    return 0;
}

void Write64(uint32_t reg, uint64_t data) {
    uint32_t low = data & 0xFFFFFFFF;
    uint32_t high = data >> 32;

    Write32(reg, low);
    Write32(reg + 1, high);
}

void Redirect(uint8_t irq, uint8_t vector, uint32_t delivery) {
    Write64(IO_APIC_RED_TABLE_ENT(irq), delivery | vector);
}

int Initialize() {
    if (!base) {
        Log::Error("[APIC] Attempted to initialize I/O APIC without setting base");
        return 1;
    }

    virtualBase = Memory::GetIOMapping(base);

    registerSelect = (uint32_t*)(virtualBase + IO_APIC_REGSEL);
    ioWindow = (uint32_t*)(virtualBase + IO_APIC_WIN);

    interrupts = Read32(IO_APIC_REGISTER_VER) >> 16;
    apicID = Read32(IO_APIC_REGISTER_ID) >> 24;

    if (debugLevelInterrupts >= DebugLevelNormal) {
        Log::Info("[APIC] I/O APIC Base %x (%x), Available Interrupts: %d, ID: %d ", base, virtualBase, interrupts,
                  apicID);
    }

    for (unsigned i = 0; i < ACPI::isos->get_length(); i++) {
        apic_iso_t* iso = ACPI::isos->get_at(i);

        if (debugLevelInterrupts >= DebugLevelVerbose) {
            Log::Info("[APIC] Interrupt Source Override, IRQ: %d, GSI: %d", iso->irqSource, iso->gSI);
        }

        Redirect(iso->gSI, iso->irqSource + 0x20, /*ICR_MESSAGE_TYPE_LOW_PRIORITY*/ 0);
    }

    return 0;
}

void SetBase(uintptr_t newBase) { base = newBase; }

void MapLegacyIRQ(uint8_t irq) {
    if (debugLevelInterrupts >= DebugLevelVerbose) {
        Log::Info("[APIC] Mapping Legacy IRQ, IRQ: %d", irq);
    }
    for (unsigned i = 0; i < ACPI::isos->get_length(); i++) {
        apic_iso_t* iso = ACPI::isos->get_at(i);
        if (iso->irqSource == irq)
            return; // We should have already redirected this IRQ
    }
    Redirect(irq, irq + 0x20, ICR_MESSAGE_TYPE_LOW_PRIORITY);
}
} // namespace IO

int Initialize() {
    cpuid_info_t cpuid = CPUID();

    if (!(cpuid.features_edx & CPUID_EDX_APIC)) {
        Log::Error("APIC Not Present");
        return 1;
    }

    asm("cli");

    IDT::DisablePIC();

    Local::Initialize();
    IO::Initialize();

    asm("sti");

    return 0;
}
} // namespace APIC

extern "C" void LocalAPICEOI() { APIC_WRITE(LOCAL_APIC_EOI, 0); }