#include <stdint.h>

#include <CPU.h>

namespace SMP{
    extern CPU* cpus[];
    extern unsigned processorCount;

    void InitializeCPU(uint16_t id);
    void InitializeCPU0Context();
    void Initialize();
}