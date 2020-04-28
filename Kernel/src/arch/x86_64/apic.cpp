#include <apic.h>

#include <cpuid.h>
#include <logging.h>
#include <paging.h>

namespace APIC{
    namespace Local {
        uintptr_t base;
        uintptr_t virtualBase;

        uint64_t ReadBase(){
            uint64_t low;
            uint64_t high;
            asm("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1B));

            return (high << 32) | low;
        }

        int Initialize(){
            Local::base = ReadBase() & LOCAL_APIC_BASE;
            virtualBase = Memory::GetIOMapping(base);

            Log::Info("[APIC] Local APIC Base %x", base);

            
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
    }
}