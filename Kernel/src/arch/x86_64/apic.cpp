#include <apic.h>

#include <cpuid.h>
#include <logging.h>
#include <paging.h>
#include <smp.h>
#include <idt.h>

namespace APIC{
    namespace Local {
        uintptr_t base;
        uintptr_t virtualBase;

        void SpuriousInterruptHandler(regs64_t* r){
            Log::Warning("[APIC] Spurious Interrupt");
        }

        uint64_t ReadBase(){
            uint64_t low;
            uint64_t high;
            asm("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1B));

            return (high << 32) | low;
        }

        void WriteBase(uint64_t val){
            uint64_t low = val & 0xFFFFFFFF;
            uint64_t high = val >> 32;
            asm("rdmsr" :: "a"(low), "d"(high), "c"(0x1B));
        }

        int Initialize(){
            Local::base = ReadBase() & LOCAL_APIC_BASE;
            virtualBase = Memory::GetIOMapping(base);

            Log::Info("[APIC] Local APIC Base %x (%x)", base, virtualBase);

            IDT::RegisterInterruptHandler(0xFF, SpuriousInterruptHandler);

            WriteBase(ReadBase() | (1UL << 11));

            //IDT::DisablePIC();

            //Read(LOCAL_APIC_SIVR);
            //Write(LOCAL_APIC_SIVR, Read(LOCAL_APIC_SIVR) |  0x1FF  /* Enable APIC, Vector 255*/);
        }

        volatile uint32_t Read(uint32_t off){
            return *((volatile uint32_t*)(virtualBase + off));
        }
        
        void Write(uint32_t off, uint32_t val){
            *((uint32_t*)(virtualBase + off)) = val;
        }

        void SendIPI(uint8_t apicID, uint32_t type, uint8_t vector){
            uint32_t high = ((uint32_t)apicID) << 24;
            uint32_t low = type | ICR_VECTOR(vector);

            Write(LOCAL_APIC_ICR_HIGH, high);
            Write(LOCAL_APIC_ICR_LOW, low);
        }
    }

    namespace IO {

    }

    int Initialize(){
        cpuid_info_t cpuid = CPU::CPUID();

        if(!(cpuid.features_edx & CPUID_EDX_APIC)) {
            Log::Error("APIC Not Present");
            return 1;
        }

        Local::Initialize();

        //CPU::InitializeCPU(1);
    }
}