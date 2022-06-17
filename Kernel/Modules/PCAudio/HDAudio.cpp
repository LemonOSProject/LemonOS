#include "HDAudio.h"

namespace Audio {

IntelHDAudioController::IntelHDAudioController(const PCIInfo& info) : PCIDevice(info) {
    SetDeviceName("Intel HD Audio Controller");

    uintptr_t bar = GetBaseAddressRegister(0);
    cRegs = reinterpret_cast<ControllerRegs*>(Memory::KernelAllocate4KPages(4));

    Memory::KernelMapVirtualMemory4K(bar, (uintptr_t)cRegs, 4);

    cRegs->globalControl = 1; // Set CRST bit to leave reset mode
    while (!(cRegs->globalControl & 0x1))
        ;

    Log::Info("[HDAudio] Initializing Intel HD Audio Controller (Base: %x, Virtual Base: %x, Global Control: %x)", bar,
              cRegs, cRegs->globalControl);
}
}