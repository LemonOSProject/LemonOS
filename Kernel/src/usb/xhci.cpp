#include <xhci.h>

#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <idt.h>

#include <video.h>

namespace USB{
    namespace XHCI{
        pci_device_t xhciControllerPci = {
            .deviceName = "Generic XHCI Controller",
            .classCode = 0x0C,
            .subclass = 0x03,
            .generic = true,
        };

        uintptr_t xhciBaseAddress;
        uintptr_t xhciVirtualAddress;

        xhci_cap_regs_t* capRegs;
        xhci_op_regs_t* opRegs;
        xhci_port_regs_t* portRegs;

        uint64_t devContextBaseAddressArrayPhys;
        uint64_t* devContextBaseAddressArray;

        void IRQHandler(regs64_t* r){

        }

        int Initialize(){
            xhciControllerPci = PCI::RegisterPCIDevice(xhciControllerPci);

            if(xhciControllerPci.vendorID == 0xFFFF || xhciControllerPci.header0.progIF != 0x30) {
                Log::Warning("No XHCI Controller Found!");
                return 1;
            }

            PCI::Config_WriteWord(xhciControllerPci.bus, xhciControllerPci.slot, xhciControllerPci.func, 0x4, xhciControllerPci.header0.command | PCI_CMD_BUS_MASTER);

            xhciBaseAddress = (uint64_t)(xhciControllerPci.header0.baseAddress0 & 0xFFFFFFF0) | (((uint64_t)xhciControllerPci.header0.baseAddress1) << 32);
            xhciVirtualAddress = Memory::GetIOMapping(xhciBaseAddress);

            //IDT::RegisterInterruptHandler(IRQ0 + 11, IRQHandler);

            capRegs = (xhci_cap_regs_t*)xhciVirtualAddress;
            opRegs = (xhci_op_regs_t*)(xhciVirtualAddress + capRegs->capLength);

            int timer = 0xFFFF;

            while(timer-- && (opRegs->usbStatus & USB_STS_CNR));
            if((opRegs->usbStatus & USB_STS_CNR)){
                Log::Error("[XHCI] Controller Timed Out");
                return 2;
            }

            //opRegs->SetMaxSlotsEnabled(255);

            devContextBaseAddressArray = reinterpret_cast<uint64_t*>(Memory::KernelAllocate4KPages(1));
            devContextBaseAddressArrayPhys = Memory::AllocatePhysicalMemoryBlock();
            Memory::KernelMapVirtualMemory4K(devContextBaseAddressArrayPhys, reinterpret_cast<uintptr_t>(devContextBaseAddressArray), 1);

            memset(devContextBaseAddressArray, 0, PAGE_SIZE_4K);

            Log::Info("[XHCI] Interface version: %d, Page size: %d", capRegs->hciVersion, opRegs->pageSize);
            
            return 0;
        }
    }
}