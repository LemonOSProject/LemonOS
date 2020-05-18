#include <stdint.h>

#include <cpu.h>

namespace SMP{
    extern CPU* cpus[];
    extern unsigned processorCount;

    void InitializeCPU(uint16_t id);
    void Initialize();
}