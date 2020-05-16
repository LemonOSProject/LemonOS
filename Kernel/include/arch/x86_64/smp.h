#include <stdint.h>

#include <cpu.h>

namespace SMP{
    extern CPU* cpus[];

    void InitializeCPU(uint16_t id);
    void Initialize();
}