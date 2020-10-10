#include <xhci.h>

#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <idt.h>
#include <memory.h>
#include <assert.h>
#include <timer.h>

namespace USB{
    Vector<XHCIController*> xhciControllers;
    uint8_t xhciClassCode = PCI_CLASS_SERIAL_BUS;
    uint8_t xhciSubclass = PCI_SUBCLASS_USB;
    uint8_t xhciProgIF = PCI_PROGIF_XHCI;

    void XHCIIRQHandler(regs64_t* r){
        Log::Info("[XHCI] Interrupt!");
    }

    int XHCIController::Initialize(){
        if(!PCI::FindGenericDevice(xhciClassCode, xhciSubclass)){
            Log::Warning("[XHCI] No XHCI Controllers Found!");
            return 1;
        }

        List<PCIDevice*> devices = PCI::GetGenericPCIDevices(xhciClassCode, xhciSubclass);
        for(PCIDevice* device : devices){
            if(device->GetProgIF() == xhciProgIF){
                xhciControllers.add_back(new XHCIController(device));
            }
        } 
        return 0;
    }
    
    XHCIController::XHCIController(PCIDevice* dev) : pciDevice(*dev){
        xhciBaseAddress = pciDevice.GetBaseAddressRegister(0);
        pciDevice.EnableBusMastering();

        xhciVirtualAddress = Memory::GetIOMapping(xhciBaseAddress);

        capRegs = reinterpret_cast<xhci_cap_regs_t*>(xhciVirtualAddress);
        opRegs = reinterpret_cast<xhci_op_regs_t*>(xhciVirtualAddress + capRegs->capLength);
        portRegs = reinterpret_cast<xhci_port_regs_t*>(xhciVirtualAddress + capRegs->capLength + 0x400); // Port regs at opBase + 0x400
        runtimeRegs = reinterpret_cast<xhci_runtime_regs_t*>(xhciVirtualAddress + (capRegs->rtsOff & ~0x1F));
        doorbellRegs = reinterpret_cast<xhci_doorbell_register_t*>(xhciVirtualAddress + (capRegs->dbOff & ~0x3));

        extCapabilities = reinterpret_cast<uintptr_t>(capRegs) + capRegs->ExtendedCapabilitiesOffset();
        
        if(!TakeOwnership()){
            Log::Error("[XHCI] Failied to take controller ownership!");
            return;
        }

        opRegs->usbCommand = (opRegs->usbCommand & ~USB_CMD_RS);
        {
            int timer = 20;

            while(timer-- && !(opRegs->usbStatus & USB_STS_HCH)) Timer::Wait(1);
            if(!(opRegs->usbStatus & USB_STS_HCH)){
                Log::Error("[XHCI] Controller not halted");
                return;
            }
        }

        opRegs->usbCommand |= USB_CMD_HCRST;
        Timer::Wait(10);
        {
            int timer = 20;

            while(timer-- && (opRegs->usbStatus & USB_STS_CNR)) Timer::Wait(1);
            if((opRegs->usbStatus & USB_STS_CNR)){
                Log::Error("[XHCI] Controller Timed Out");
                return;
            }
        }

        controllerIRQ = pciDevice.AllocateVector(PCIVectorAny);
        if(controllerIRQ == 0xFF){
            Log::Error("[XHCI] Failed to allocate IRQ!");
            return;
        }

        IDT::RegisterInterruptHandler(controllerIRQ, XHCIIRQHandler);

        devContextBaseAddressArrayPhys = Memory::AllocatePhysicalMemoryBlock();
        devContextBaseAddressArray = reinterpret_cast<uint64_t*>(Memory::GetIOMapping(devContextBaseAddressArrayPhys));

        memset(devContextBaseAddressArray, 0, PAGE_SIZE_4K);

        opRegs->devContextBaseAddrArrayPtr = devContextBaseAddressArrayPhys;

        // If the Max Scratchpad Buffers > 0 then the first entry in the DCBAA shall contain a pointer to the Scratchpad Buffer Array
        if(capRegs->MaxScratchpadBuffers() > 0){
            scratchpadBuffersPhys = Memory::AllocatePhysicalMemoryBlock();
            scratchpadBuffers = reinterpret_cast<uint64_t*>(Memory::GetIOMapping(devContextBaseAddressArrayPhys));

            memset(scratchpadBuffers, 0, PAGE_SIZE_4K);
            
            for(unsigned i = 0; i < capRegs->MaxScratchpadBuffers() && i < PAGE_SIZE_4K / sizeof(uint64_t); i++){
                scratchpadBuffers[i] = Memory::AllocatePhysicalMemoryBlock();
            }

            devContextBaseAddressArray[0] = scratchpadBuffersPhys;
        }

        cmdRingPointer = reinterpret_cast<uint8_t*>(Memory::KernelAllocate4KPages(1));
        cmdRingPointerPhys = Memory::AllocatePhysicalMemoryBlock();
        Memory::KernelMapVirtualMemory4K(cmdRingPointerPhys, reinterpret_cast<uintptr_t>(cmdRingPointer), 1, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

        memset(cmdRingPointer, 0, PAGE_SIZE_4K);

        opRegs->cmdRingCtl = 0; // Clear everything
        opRegs->cmdRingCtl = cmdRingPointerPhys | 1;

        opRegs->deviceNotificationControl = 2; // FUNCTION_WAKE notification, all others should be handled automatically

        maxSlots = 40;
        if(maxSlots > capRegs->MaxSlots()){
            maxSlots = capRegs->MaxSlots();
        }
        
        ports = new XHCIPort[capRegs->MaxPorts()];
        InitializeProtocols();

        interrupter = &runtimeRegs->interrupters[0];
        interrupter->interruptEnable = 1;
        interrupter->intModerationInterval = 4000; // (~1ms)
        
        eventRingSegmentTablePhys = Memory::AllocatePhysicalMemoryBlock();
        eventRingSegmentTable = reinterpret_cast<xhci_event_ring_segment_table_entry_t*>(Memory::KernelAllocate4KPages(1));
        Memory::KernelMapVirtualMemory4K(eventRingSegmentTablePhys, reinterpret_cast<uintptr_t>(eventRingSegmentTable), 1, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);
        memset(eventRingSegmentTable, 0, PAGE_SIZE_4K);
        
        eventRingSegmentTableSize = 1; // For now we support one segment // PAGE_SIZE_4K / XHCI_EVENT_RING_SEGMENT_TABLE_ENTRY_SIZE;
        eventRingSegments = new XHCIEventRingSegment[eventRingSegmentTableSize];
        for(unsigned i = 0; i < eventRingSegmentTableSize; i++){
            xhci_event_ring_segment_table_entry_t* entry = &eventRingSegmentTable[i];
            XHCIEventRingSegment* segment = &eventRingSegments[i];
            
            segment->entry = entry;
            segment->segmentPhys = Memory::AllocatePhysicalMemoryBlock();
            segment->segment = reinterpret_cast<uint8_t*>(Memory::KernelAllocate4KPages(1));
            Memory::KernelMapVirtualMemory4K(segment->segmentPhys, reinterpret_cast<uintptr_t>(segment->segment), 1, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

            assert(!(segment->segmentPhys & 0xFFF));
            entry->ringSegmentBaseAddress = segment->segmentPhys;

            entry->ringSegmentSize = PAGE_SIZE_4K / XHCI_TRB_SIZE; // Comes down to 256 TRBs per segment
        }

        interrupter->eventRingSegmentTableSize = eventRingSegmentTableSize;
        interrupter->eventRingSegmentTableBaseAddress = eventRingSegmentTablePhys;
        interrupter->eventRingDequeuePointer = eventRingSegmentTablePhys;
        opRegs->SetMaxSlotsEnabled(maxSlots);

        Log::Info("[XHCI] Interface version: %x, Page size: %d, Operational registers offset: %x, Runtime registers offset: %x, Doorbell registers offset: %x", capRegs->hciVersion, opRegs->pageSize, capRegs->capLength, capRegs->rtsOff, capRegs->dbOff);
        Log::Info("[XHCI] MaxSlots: %x, Max Scratchpad Buffers: %x", capRegs->MaxSlots(), capRegs->MaxScratchpadBuffers());

        opRegs->usbCommand |= USB_CMD_HSEE | USB_CMD_INTE | USB_CMD_RS;
        {
            int timer = 20;

            while(timer-- && (opRegs->usbStatus & USB_STS_HCH)) Timer::Wait(1);
            if((opRegs->usbStatus & USB_STS_HCH)){
                Log::Error("[XHCI] Controller Halted");
                return;
            }
        }

        opRegs->usbStatus &= ~USB_STS_EINT;
        interrupter->interruptPending = 0;
        interrupter->interruptEnable = 1;

        xhci_noop_command_trb_t testCommand;
        testCommand.trbType = TRBTypes::TRBTypeNoOpCommand;

        memcpy(cmdRingPointer, &testCommand, XHCI_TRB_SIZE);
        doorbellRegs[0].target = 0;

        InitializePorts();
    }

    bool XHCIController::TakeOwnership(){
        volatile xhci_ext_cap_legacy_support_t* cap = reinterpret_cast<xhci_ext_cap_legacy_support_t*>(extCapabilities); // Legacy support should be the first entry in the extended capabilities
        
        if(cap->capID != XHCIExtendedCapabilities::XHCIExtCapLegacySupport){
            return true; // If there is no legacy support capability don't worry
        }

        if(cap->controllerBIOSSemaphore){
            Log::Info("[XHCI] BIOS owns host controller, attempting to take ownership...");
            cap->controllerOSSemaphore = 1;
        }

        int timer = 20;
        while(cap->controllerBIOSSemaphore && timer--){
            Timer::Wait(1); // Wait 20ms to acquire controller
        }

        if(cap->controllerBIOSSemaphore){
            return false;
        }

        return true;
    }

    void XHCIController::InitializeProtocols(){
        auto initializeProto = [&](xhci_ext_cap_supported_protocol_t* cap){
            Log::Info("[XHCI] Initializing protocol \"%c%c%c%c\", Version: %d%d.%d%d, Port Range: %u-%u", cap->name[0], cap->name[1], cap->name[2], cap->name[3], (cap->majorRevision >> 4), (cap->majorRevision & 0x9), (cap->minorRevision >> 4), (cap->minorRevision & 0x9), cap->portOffset, (cap->portOffset + cap->portCount));

            protocols.add_back(cap);

            for(unsigned i = cap->portOffset; i < cap->portOffset + cap->portCount && i < capRegs->MaxPorts(); i++){
                ports[i].protocol = cap;
            }
        };

        xhci_ext_cap_supported_protocol_t* cap = reinterpret_cast<xhci_ext_cap_supported_protocol_t*>(extCapabilities);
        for(;;){
            if(cap->capID == XHCIExtendedCapabilities::XHCIExtCapSupportedProtocol){
                initializeProto(cap);
            }

            if(!cap->nextCap){
                break; // Last capability
            }

            cap = reinterpret_cast<xhci_ext_cap_supported_protocol_t*>(reinterpret_cast<uintptr_t>(cap) + (cap->nextCap << 2));
        }
    }

    void XHCIController::InitializePorts(){
        for(xhci_ext_cap_supported_protocol_t* protocol : protocols){ // Check both protocols as some share ports
            for(unsigned i = protocol->portOffset; i < protocol->portOffset + protocol->portCount && i < capRegs->MaxPorts() + 1U; i++){
                xhci_port_regs_t& port = portRegs[i - 1];
                if(!port.Powered()){
                    port.PowerOn();

                    Timer::Wait(20); // 20ms delay

                    if(!port.Powered()){
                        continue; // Not powered so move on
                    }
                }

                if(protocol->majorRevision == 0x3){ // USB 3.0
                    port.WarmReset();
                } else {
                    port.Reset();
                }

                if(port.Connected() && port.Enabled()){
                    Log::Info("Port %i is connected and enabled!", i);
                }
            }
        }
    }
}