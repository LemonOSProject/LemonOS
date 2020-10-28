#include <xhci.h>

#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <idt.h>
#include <memory.h>
#include <assert.h>
#include <timer.h>

#include <debug.h>

namespace USB{
    Vector<XHCIController*> xhciControllers;
    uint8_t xhciClassCode = PCI_CLASS_SERIAL_BUS;
    uint8_t xhciSubclass = PCI_SUBCLASS_USB;
    uint8_t xhciProgIF = PCI_PROGIF_XHCI;

    void XHCIIRQHandler(XHCIController* xHC, regs64_t* r){
        xHC->OnInterrupt();
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
        pciDevice.EnableInterrupts();
        pciDevice.EnableMemorySpace();

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

        IDT::RegisterInterruptHandler(controllerIRQ, reinterpret_cast<isr_t>(&XHCIIRQHandler), this);

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

        commandRing = reinterpret_cast<xhci_trb_t*>(Memory::KernelAllocate4KPages(1));
        cmdRingPointerPhys = Memory::AllocatePhysicalMemoryBlock();
        Memory::KernelMapVirtualMemory4K(cmdRingPointerPhys, reinterpret_cast<uintptr_t>(commandRing), 1, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

        commandRingIndexMax = PAGE_SIZE_4K / XHCI_TRB_SIZE;
        memset(commandRing, 0, PAGE_SIZE_4K);

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
        //interrupter->intModerationInterval = 4000; // (~1ms)
        
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
            segment->segment = reinterpret_cast<xhci_event_trb_t*>(Memory::KernelAllocate4KPages(1));
            Memory::KernelMapVirtualMemory4K(segment->segmentPhys, reinterpret_cast<uintptr_t>(segment->segment), 1, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

            entry->ringSegmentBaseAddress = segment->segmentPhys;
            memset(segment->segment, 0, PAGE_SIZE_4K);

            entry->ringSegmentSize = PAGE_SIZE_4K / XHCI_TRB_SIZE; // Comes down to 256 TRBs per segment
            segment->size = PAGE_SIZE_4K / XHCI_TRB_SIZE;
        }
        eventRingDequeue = eventRingSegments[0].segment;

        interrupter->eventRingSegmentTableSize = eventRingSegmentTableSize;
        interrupter->eventRingSegmentTableBaseAddress = eventRingSegmentTablePhys;
        interrupter->eventRingDequeuePointer = reinterpret_cast<uintptr_t>(eventRingSegments[0].segment);
        opRegs->SetMaxSlotsEnabled(maxSlots);

        Log::Info("[XHCI] Interface version: %x, Page size: %d, Operational registers offset: %x, Runtime registers offset: %x, Doorbell registers offset: %x", capRegs->hciVersion, opRegs->pageSize, capRegs->capLength, capRegs->rtsOff, capRegs->dbOff);
        
        if(debugLevelXHCI >= DebugLevelNormal){
            Log::Info("[XHCI] MaxSlots: %x, Max Scratchpad Buffers: %x", capRegs->MaxSlots(), capRegs->MaxScratchpadBuffers());
        }

        opRegs->usbCommand |= USB_CMD_HSEE | USB_CMD_INTE | USB_CMD_RS;
        {
            int timer = 20;

            while(timer-- && (opRegs->usbStatus & USB_STS_HCH)) Timer::Wait(1);
            if((opRegs->usbStatus & USB_STS_HCH)){
                Log::Error("[XHCI] Controller Halted");
                return;
            }
        }

        interrupter->interruptEnable = 1;
        interrupter->interruptPending = 0;
        interrupter->eventHandlerBusy = 0;
        opRegs->usbStatus &= ~USB_STS_EINT;

        xhci_noop_command_trb_t testCommand;
        memset(&testCommand, 0, sizeof(xhci_noop_command_trb_t));
        testCommand.trbType = TRBTypes::TRBTypeNoOpCommand;
        testCommand.cycleBit = 1;

        SendCommand(&testCommand);
        doorbellRegs[0].target = 0;

        InitializePorts();
    }

    void XHCIController::SendCommand(void* data){
        xhci_trb_t* command = reinterpret_cast<xhci_trb_t*>(data);

        if(commandRingCycleState)
            command->cycleBit = 1;
        else command->cycleBit = 0;

        commandRing[commandRingEnqueueIndex++] = *command;

        if(commandRingEnqueueIndex >= commandRingIndexMax - 1){
            xhci_link_trb_t lnkTrb;

            memset(&lnkTrb, 0, sizeof(xhci_link_trb_t));
            
            if(commandRingCycleState)
                lnkTrb.cycleBit = 1;
            else lnkTrb.cycleBit = 0;

            lnkTrb.interrupterTarget = 0;
            lnkTrb.interruptOnCompletion = 0;
            lnkTrb.segmentPtr = cmdRingPointerPhys;
            lnkTrb.toggleCycle = 1;

            commandRing[commandRingEnqueueIndex] = *reinterpret_cast<xhci_trb_t*>(&lnkTrb);
            commandRingEnqueueIndex = 0;

            commandRingCycleState = !commandRingCycleState;
        }
    }

    void XHCIController::EnableSlot(){
        xhci_enable_slot_command_trb_t trb;
        memset(&trb, 0, XHCI_TRB_SIZE);

        trb.trbType = TRBTypes::TRBTypeEnableSlotCommand;

        SendCommand(&trb);
    }

    void XHCIController::OnInterrupt(){
        Log::Info("[XHCI] Interrupt!");

        if(!eventRingSegments) return;

        XHCIEventRingSegment* segment = &eventRingSegments[0];
        xhci_event_trb_t* event = eventRingDequeue;

        if(debugLevelXHCI >= DebugLevelVerbose){
            Log::Info("cycle: %Y event cycle: %Y", event->cycleBit, eventRingCycleState);
        }

        while(!!event->cycleBit == !!eventRingCycleState){
            if(debugLevelXHCI >= DebugLevelVerbose){
                Log::Info("[XHCI] Received event (TRB Type: %x (%x))", event->trbType, (((uint32_t*)event)[3] >> 10) & 0x3f);
            }

            event++;
            uintptr_t diff = reinterpret_cast<uintptr_t>(event) - reinterpret_cast<uintptr_t>(segment->segment);
            if(!diff || !((diff) % (segment->size * XHCI_TRB_SIZE))){ // Check if we are at either beginning or end of ring segment
                eventRingDequeue = segment->segment;
                event = eventRingDequeue;
                eventRingCycleState = !eventRingCycleState;
                break;
            }
        }

        interrupter->eventRingDequeuePointer = reinterpret_cast<uintptr_t>(eventRingDequeue) | XHCI_INT_ERDP_BUSY;
        interrupter->interruptPending = 0;
        opRegs->usbStatus &= ~USB_STS_EINT;
    }

    bool XHCIController::TakeOwnership(){
        volatile xhci_ext_cap_legacy_support_t* cap = reinterpret_cast<xhci_ext_cap_legacy_support_t*>(extCapabilities); // Legacy support should be the first entry in the extended capabilities
        
        if(cap->capID != XHCIExtendedCapabilities::XHCIExtCapLegacySupport){
            return true; // If there is no legacy support capability don't worry
        }

        if(cap->controllerBIOSSemaphore){
            if(debugLevelXHCI >= DebugLevelVerbose){
                Log::Info("[XHCI] BIOS owns host controller, attempting to take ownership...");
            }

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
            if(debugLevelXHCI){
                Log::Info("[XHCI] Initializing protocol \"%c%c%c%c\", Version: %u%u.%u%u, Port Range: %u-%u", cap->name[0], cap->name[1], cap->name[2], cap->name[3], (cap->majorRevision >> 4), (cap->majorRevision & 0xF), (cap->minorRevision >> 4), (cap->minorRevision & 0xF), cap->portOffset, (cap->portOffset + cap->portCount));
            }

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

        if(!protocols.get_length()){
            Log::Error("[XHCI] Could not find any protocols!");
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

                if(!port.Enabled()){
                    int timeout = 25;
                    while(timeout-- && !(port.portSC & XHCI_PORTSC_PRC)){
                        Timer::Wait(1); // Wait for port reset change
                    }
                }

                if(port.Enabled()){
                    if(debugLevelXHCI >= DebugLevelVerbose){
                        Log::Info("Port %i is enabled!", i);
                    }
                }
            }
        }
    }
}