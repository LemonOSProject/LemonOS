#include <xhci.h>

#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <idt.h>
#include <memory.h>
#include <assert.h>

#include <video.h>

namespace USB{
    PCIDevice* xhciPCIDevice;
    uint8_t xhciClassCode = PCI_CLASS_SERIAL_BUS;
    uint8_t xhciSubclass = PCI_SUBCLASS_USB;

    int XHCIController::Initialize(){
        if(!PCI::FindGenericDevice(xhciClassCode, xhciSubclass)){
            Log::Warning("[XHCI] No XHCI Controller Found!");
            return 1;
        }

        xhciPCIDevice = &PCI::GetGenericPCIDevice(xhciClassCode, xhciSubclass);
        assert(xhciPCIDevice->vendorID != 0xFFFF);

        if(xhciPCIDevice->GetProgIF() != 0x30) {
            Log::Warning("[XHCI] Other USB Controller Found!");
            return 1;
        }

        xhciPCIDevice->EnableBusMastering();

        new XHCIController(xhciPCIDevice->GetBaseAddressRegister(0));        
        return 0;
    }
    
    XHCIController::XHCIController(uintptr_t baseAddress){
        xhciBaseAddress = baseAddress;
        xhciVirtualAddress = Memory::GetIOMapping(xhciBaseAddress);

        //IDT::RegisterInterruptHandler(IRQ0 + 11, IRQHandler);

        capRegs = (xhci_cap_regs_t*)xhciVirtualAddress;
        opRegs = (xhci_op_regs_t*)(xhciVirtualAddress + capRegs->capLength);

        {
            int timer = 0xFFFF;

            while(timer-- && (opRegs->usbStatus & USB_STS_CNR));
            if((opRegs->usbStatus & USB_STS_CNR)){
                Log::Error("[XHCI] Controller Timed Out");
                return;
            }
        }

        devContextBaseAddressArrayPhys = Memory::AllocatePhysicalMemoryBlock();
        devContextBaseAddressArray = reinterpret_cast<uint64_t*>(Memory::GetIOMapping(devContextBaseAddressArrayPhys));

        memset(devContextBaseAddressArray, 0, PAGE_SIZE_4K);

        opRegs->devContextBaseAddrArrayPtr = devContextBaseAddressArrayPhys;

        cmdRingPointer = reinterpret_cast<uint64_t*>(Memory::KernelAllocate4KPages(1));
        cmdRingPointerPhys = Memory::AllocatePhysicalMemoryBlock();
        Memory::KernelMapVirtualMemory4K(cmdRingPointerPhys, reinterpret_cast<uintptr_t>(cmdRingPointer), 1);

        memset(cmdRingPointer, 0, PAGE_SIZE_4K);

        opRegs->cmdRingCtl = 0; // Clear everything
        opRegs->cmdRingCtl = cmdRingPointerPhys;

        opRegs->SetMaxSlotsEnabled(40);

        Log::Info("[XHCI] Interface version: %x, Page size: %d, Operational registers offset: %x, Runtime registers offset: %x, Doorbell registers offset: %x", capRegs->hciVersion, opRegs->pageSize, capRegs->capLength, capRegs->rtsOff, capRegs->dbOff);

        opRegs->usbCommand = opRegs->usbCommand | USB_CMD_RS;
        {
            int timer = 0xFFFF;

            while(timer-- && (opRegs->usbStatus & USB_STS_HCH));
            if((opRegs->usbStatus & USB_STS_HCH)){
                Log::Error("[XHCI] Controller Halted");
                return;
            }
        }
    }
}