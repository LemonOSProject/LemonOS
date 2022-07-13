#include <USB/XHCI.h>

#include <Assert.h>
#include <IDT.h>
#include <Logging.h>
#include <Memory.h>
#include <PCI.h>
#include <Paging.h>
#include <Timer.h>

#include <Debug.h>

namespace USB {
Vector<XHCIController*> xhciControllers;
uint8_t xhciClassCode = PCI_CLASS_SERIAL_BUS;
uint8_t xhciSubclass = PCI_SUBCLASS_USB;
uint8_t xhciProgIF = PCI_PROGIF_XHCI;

void XHCIIRQHandler(XHCIController* xHC, RegisterContext* r) { xHC->OnInterrupt(); }

int XHCIController::Initialize() {
    if (!PCI::FindGenericDevice(xhciClassCode, xhciSubclass)) {
        Log::Warning("[XHCI] No XHCI Controllers Found!");
        return 1;
    }

    return 0;

    /*PCI::EnumerateGenericPCIDevices(xhciClassCode, xhciSubclass, [](const PCIInfo& dev) -> void {
        if (dev.progIf == xhciProgIF) {
            xhciControllers.add_back(new XHCIController(dev));
        }
    });
    return 0;*/
}

XHCIController::XHCIController(const PCIInfo& dev) : PCIDevice(dev) {
    xhciBaseAddress = GetBaseAddressRegister(0);
    EnableBusMastering();
    EnableInterrupts();
    EnableMemorySpace();

    xhciVirtualAddress = Memory::GetIOMapping(xhciBaseAddress);

    capRegs = reinterpret_cast<xhci_cap_regs_t*>(xhciVirtualAddress);
    opRegs = reinterpret_cast<xhci_op_regs_t*>(xhciVirtualAddress + capRegs->capLength);
    portRegs = reinterpret_cast<xhci_port_regs_t*>(xhciVirtualAddress + capRegs->capLength +
                                                   0x400); // Port regs at opBase + 0x400
    runtimeRegs = reinterpret_cast<xhci_runtime_regs_t*>(xhciVirtualAddress + (capRegs->rtsOff & ~0x1F));
    doorbellRegs = reinterpret_cast<xhci_doorbell_register_t*>(xhciVirtualAddress + (capRegs->dbOff & ~0x3));

    extCapabilities = reinterpret_cast<uintptr_t>(capRegs) + capRegs->ExtendedCapabilitiesOffset();

    if (!TakeOwnership()) {
        Log::Error("[XHCI] Failied to take controller ownership!");
        return;
    }

    opRegs->usbCommand = (opRegs->usbCommand & ~USB_CMD_RS);
    {
        int timer = 20;

        while (timer-- && !(opRegs->usbStatus & USB_STS_HCH))
            Timer::Wait(1);
        if (!(opRegs->usbStatus & USB_STS_HCH)) {
            Log::Error("[XHCI] Controller not halted");
            return;
        }
    }

    opRegs->usbCommand |= USB_CMD_HCRST;
    Timer::Wait(10);
    {
        int timer = 20;

        while (timer-- && (opRegs->usbStatus & USB_STS_CNR))
            Timer::Wait(1);
        if ((opRegs->usbStatus & USB_STS_CNR)) {
            Log::Error("[XHCI] Controller Timed Out");
            return;
        }
    }

    controllerIRQ = AllocateVector(PCIVectorAny);
    if (controllerIRQ == 0xFF) {
        Log::Error("[XHCI] Failed to allocate IRQ!");
        return;
    }

    IDT::RegisterInterruptHandler(controllerIRQ, reinterpret_cast<isr_t>(&XHCIIRQHandler), this);

    devContextBaseAddressArrayPhys = Memory::AllocatePhysicalMemoryBlock();
    devContextBaseAddressArray = reinterpret_cast<uint64_t*>(Memory::GetIOMapping(devContextBaseAddressArrayPhys));

    memset(devContextBaseAddressArray, 0, PAGE_SIZE_4K);

    opRegs->devContextBaseAddrArrayPtr = devContextBaseAddressArrayPhys;

    // If the Max Scratchpad Buffers > 0 then the first entry in the DCBAA shall contain a pointer to the Scratchpad
    // Buffer Array
    if (capRegs->MaxScratchpadBuffers() > 0) {
        scratchpadBuffersPhys = Memory::AllocatePhysicalMemoryBlock();
        scratchpadBuffers = reinterpret_cast<uint64_t*>(Memory::GetIOMapping(devContextBaseAddressArrayPhys));

        memset(scratchpadBuffers, 0, PAGE_SIZE_4K);

        for (unsigned i = 0; i < capRegs->MaxScratchpadBuffers() && i < PAGE_SIZE_4K / sizeof(uint64_t); i++) {
            scratchpadBuffers[i] = Memory::AllocatePhysicalMemoryBlock();
        }

        devContextBaseAddressArray[0] = scratchpadBuffersPhys;
    }

    opRegs->cmdRingCtl = 0; // Clear everything
    opRegs->cmdRingCtl = commandRing.physicalAddr | 1;

    //opRegs->deviceNotificationControl = 2; // FUNCTION_WAKE notification, all others should be handled automatically

    maxSlots = 40;
    if (maxSlots > capRegs->MaxSlots()) {
        maxSlots = capRegs->MaxSlots();
    }

    ports = new XHCIPort[capRegs->MaxPorts()];

    InitializeProtocols();

    interrupter = &runtimeRegs->interrupters[0];
    // interrupter->intModerationInterval = 4000; // (~1ms)

    /*eventRingSegmentTablePhys = Memory::AllocatePhysicalMemoryBlock();
    eventRingSegmentTable = reinterpret_cast<xhci_event_ring_segment_table_entry_t*>(Memory::KernelAllocate4KPages(1));
    Memory::KernelMapVirtualMemory4K(eventRingSegmentTablePhys, reinterpret_cast<uintptr_t>(eventRingSegmentTable), 1,
                                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);
    memset(eventRingSegmentTable, 0, PAGE_SIZE_4K);

    eventRingSegmentTableSize =
        1; // For now we support one segment // PAGE_SIZE_4K / XHCI_EVENT_RING_SEGMENT_TABLE_ENTRY_SIZE;
    eventRingSegments = new XHCIEventRingSegment[eventRingSegmentTableSize];
    for (unsigned i = 0; i < eventRingSegmentTableSize; i++) {
        xhci_event_ring_segment_table_entry_t* entry = &eventRingSegmentTable[i];
        XHCIEventRingSegment* segment = &eventRingSegments[i];

        segment->entry = entry;
        segment->segmentPhys = Memory::AllocatePhysicalMemoryBlock();
        segment->segment = reinterpret_cast<xhci_event_trb_t*>(Memory::KernelAllocate4KPages(1));
        Memory::KernelMapVirtualMemory4K(segment->segmentPhys, reinterpret_cast<uintptr_t>(segment->segment), 1,
                                         PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

        entry->ringSegmentBaseAddress = segment->segmentPhys;
        memset(segment->segment, 0, PAGE_SIZE_4K);

        entry->ringSegmentSize = PAGE_SIZE_4K / XHCI_TRB_SIZE; // Comes down to 256 TRBs per segment
        segment->size = PAGE_SIZE_4K / XHCI_TRB_SIZE;
    }
    eventRingDequeue = eventRingSegments[0].segment;

    interrupter->eventRingSegmentTableSize = eventRingSegmentTableSize;
    interrupter->eventRingSegmentTableBaseAddress = eventRingSegmentTablePhys;
    interrupter->eventRingDequeuePointer = reinterpret_cast<uintptr_t>(eventRingSegments[0].segment);
    opRegs->SetMaxSlotsEnabled(maxSlots);*/

    opRegs->SetMaxSlotsEnabled(maxSlots);

    Log::Info("[XHCI] Interface version: %x, Page size: %d, Operational registers offset: %x, Runtime registers "
              "offset: %x, Doorbell registers offset: %x",
              capRegs->hciVersion, opRegs->pageSize, capRegs->capLength, capRegs->rtsOff, capRegs->dbOff);

    if (debugLevelXHCI >= DebugLevelNormal) {
        Log::Info("[XHCI] MaxSlots: %x, Max Scratchpad Buffers: %x", capRegs->MaxSlots(),
                  capRegs->MaxScratchpadBuffers());
    }


    interrupter->interruptEnable = 1;
    interrupter->interruptPending = 1;
    interrupter->eventRingSegmentTableSize = eventRing.segmentCount;
    interrupter->eventRingSegmentTableBaseAddress = eventRing.segmentsPhys;
    interrupter->eventRingDequeuePointer = reinterpret_cast<uintptr_t>(eventRing.segments[0].physicalAddr) | XHCI_INT_ERDP_BUSY;
    opRegs->usbStatus |= USB_STS_EINT;

    opRegs->usbCommand |= USB_CMD_HSEE | USB_CMD_INTE | USB_CMD_RS;
    {
        int timer = 20;

        while (timer-- && (opRegs->usbStatus & USB_STS_HCH))
            Timer::Wait(1);
        if ((opRegs->usbStatus & USB_STS_HCH)) {
            Log::Error("[XHCI] Controller Halted");
            return;
        }
    }

    xhci_noop_command_trb_t testCommand;
    memset(&testCommand, 0, sizeof(xhci_noop_command_trb_t));
    testCommand.trbType = TRBTypes::TRBTypeNoOpCommand;
    testCommand.cycleBit = 1;

    Log::Info("sending noop");
    commandRing.SendCommandRaw(&testCommand);
    doorbellRegs[0].doorbell = 0;
    Log::Info("sent noop");

    commandRing.SendCommandRaw(&testCommand);
    commandRing.SendCommandRaw(&testCommand);
    doorbellRegs[0].doorbell = 0;
    commandRing.SendCommandRaw(&testCommand);
    doorbellRegs[0].doorbell = 0;
    commandRing.SendCommandRaw(&testCommand);
    doorbellRegs[0].doorbell = 0;
    commandRing.SendCommandRaw(&testCommand);

    commandRing.SendCommandRaw(&testCommand);
    doorbellRegs[0].doorbell = 0;

    InitializePorts();
}

XHCIController::CommandRing::CommandRing(XHCIController* c) : hcd(c) {
    physicalAddr = Memory::AllocatePhysicalMemoryBlock();
    ring = (xhci_trb_t*)Memory::KernelAllocate4KPages(1);

    Memory::KernelMapVirtualMemory4K(physicalAddr, reinterpret_cast<uintptr_t>(ring), 1,
                                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

    memset(ring, 0, PAGE_SIZE_4K);

    enqueueIndex = 0;
    maxIndex = PAGE_SIZE_4K / XHCI_TRB_SIZE;
    maxIndex--;

    auto* trb = (xhci_link_trb_t*)(&ring[maxIndex]);
    memset(trb, 0, sizeof(xhci_link_trb_t));

    trb->trbType = TRBTypeLink;
    trb->cycleBit = 1;
    trb->interrupterTarget = 0;
    trb->interruptOnCompletion = 1;
    trb->segmentPtr = physicalAddr;
    trb->toggleCycle = 1;
    trb->chainBit = 0;

    events = new CommandCompletionEvent[maxIndex];
}

void XHCIController::CommandRing::SendCommandRaw(void* data) {
    xhci_trb_t* command = reinterpret_cast<xhci_trb_t*>(data);

    ScopedSpinLock<true> lockRing(lock);

    if (cycleState)
        command->cycleBit = 1;
    else
        command->cycleBit = 0;

    memcpy(&ring[enqueueIndex++], command, sizeof(xhci_trb_t));

    if (enqueueIndex >= maxIndex - 1) {
        cycleState = !cycleState;
    }
}

XHCIController::xhci_command_completion_event_trb_t XHCIController::CommandRing::SendCommand(void* data) {
    CommandCompletionEvent* ev;

    {
        xhci_trb_t* command = reinterpret_cast<xhci_trb_t*>(data);

        ScopedSpinLock<true> lockRing(lock);

        ev = &events[enqueueIndex];
        ev->completion.SetValue(0);

        if (cycleState)
            command->cycleBit = 1;
        else
            command->cycleBit = 0;

        ring[enqueueIndex++] = *command;
    }

    hcd->doorbellRegs[0].doorbell = 0;

    bool wasInterrupted = ev->completion.Wait();
    assert(!wasInterrupted);

    return ev->event;
}

XHCIController::EventRing::EventRing(XHCIController* c) : hcd(c) {
    segmentCount = 1;

    segmentsPhys = Memory::AllocatePhysicalMemoryBlock();
    segmentTable = (xhci_event_ring_segment_table_entry_t*)Memory::KernelAllocate4KPages(1);
    Memory::KernelMapVirtualMemory4K(segmentsPhys, reinterpret_cast<uintptr_t>(segmentTable), 1,
                                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);
    memset(segmentTable, 0, PAGE_SIZE_4K);

    segments = new EventRingSegment[segmentCount];

    for (uint32_t i = 0; i < segmentCount; i++) {
        segments[i].physicalAddr = Memory::AllocatePhysicalMemoryBlock();
        segments[i].segment = (xhci_event_trb_t*)Memory::KernelAllocate4KPages(1);
        segments[i].size = PAGE_SIZE_4K / XHCI_TRB_SIZE;

        Memory::KernelMapVirtualMemory4K(segments[i].physicalAddr, reinterpret_cast<uintptr_t>(segments[i].segment), 1,
                                         PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

        memset(segments[i].segment, 0, PAGE_SIZE_4K);

        segmentTable[i].ringSegmentBaseAddress = segments[i].physicalAddr;
        segmentTable[i].ringSegmentSize = segments[i].size;
    }
}

bool XHCIController::EventRing::Dequeue(xhci_event_trb_t* trb) {
    xhci_event_trb_t* ev = &segments[0].segment[dequeueIndex];
    if (!!ev->cycleBit == !!cycleState) {
        *trb = *ev;

        dequeueIndex++;
        if (dequeueIndex >= segments[0].size) {
            dequeueIndex = 0;
            cycleState = !cycleState;
        }

        return true;
    }

    return false;
}

void XHCIController::EnableSlot() {
    xhci_enable_slot_command_trb_t trb;
    memset(&trb, 0, XHCI_TRB_SIZE);

    trb.trbType = TRBTypes::TRBTypeEnableSlotCommand;

    commandRing.SendCommand(&trb);
}

void XHCIController::OnInterrupt() {
    Log::Info("[XHCI] Interrupt!");

    interrupter->interruptPending = 1;
    
    if (!(opRegs->usbStatus | USB_STS_EINT)) {
        return;
    }

    opRegs->usbStatus |= USB_STS_EINT;

    xhci_event_trb_t ev;
    if(eventRing.Dequeue(&ev)){
        Log::Info("recv event!");
    }

    for(int i = 0; i < eventRing.segments[0].size; i++){
        auto& trb = eventRing.segments[0].segment[i];
        if(trb.trbType == 0){
            continue;
        }

        Log::Info("cycle: %Y event cycle: %Y TRB type: %x", trb.cycleBit, eventRing.cycleState, trb.trbType);
    }

    interrupter->eventRingDequeuePointer =
        (reinterpret_cast<uintptr_t>(eventRing.segments[0].physicalAddr) + eventRing.dequeueIndex * XHCI_TRB_SIZE) |
        XHCI_INT_ERDP_BUSY;
    interrupter->interruptPending = 0;
    opRegs->usbStatus &= ~USB_STS_EINT;
}

bool XHCIController::TakeOwnership() {
    volatile xhci_ext_cap_legacy_support_t* cap = reinterpret_cast<xhci_ext_cap_legacy_support_t*>(
        extCapabilities); // Legacy support should be the first entry in the extended capabilities

    if (cap->capID != XHCIExtendedCapabilities::XHCIExtCapLegacySupport) {
        return true; // If there is no legacy support capability don't worry
    }

    if (cap->controllerBIOSSemaphore) {
        if (debugLevelXHCI >= DebugLevelVerbose) {
            Log::Info("[XHCI] BIOS owns host controller, attempting to take ownership...");
        }

        cap->controllerOSSemaphore = 1;
    }

    int timer = 20;
    while (cap->controllerBIOSSemaphore && timer--) {
        Timer::Wait(1); // Wait 20ms to acquire controller
    }

    if (cap->controllerBIOSSemaphore) {
        return false;
    }

    return true;
}

void XHCIController::InitializeProtocols() {
    auto initializeProto = [&](xhci_ext_cap_supported_protocol_t* cap) {
        if (debugLevelXHCI) {
            Log::Info("[XHCI] Initializing protocol \"%c%c%c%c\", Version: %u%u.%u%u, Port Range: %u-%u", cap->name[0],
                      cap->name[1], cap->name[2], cap->name[3], (cap->majorRevision >> 4), (cap->majorRevision & 0xF),
                      (cap->minorRevision >> 4), (cap->minorRevision & 0xF), cap->portOffset,
                      (cap->portOffset + cap->portCount));
        }

        protocols.add_back(cap);

        for (unsigned i = cap->portOffset; i < cap->portOffset + cap->portCount && i < capRegs->MaxPorts(); i++) {
            ports[i].protocol = cap;
        }
    };

    xhci_ext_cap_supported_protocol_t* cap = reinterpret_cast<xhci_ext_cap_supported_protocol_t*>(extCapabilities);
    for (;;) {
        if (cap->capID == XHCIExtendedCapabilities::XHCIExtCapSupportedProtocol) {
            initializeProto(cap);
        }

        if (!cap->nextCap) {
            break; // Last capability
        }

        cap = reinterpret_cast<xhci_ext_cap_supported_protocol_t*>(reinterpret_cast<uintptr_t>(cap) +
                                                                   (cap->nextCap << 2));
    }

    if (!protocols.get_length()) {
        Log::Error("[XHCI] Could not find any protocols!");
    }
}

void XHCIController::InitializePorts() {
    for (xhci_ext_cap_supported_protocol_t* protocol : protocols) { // Check both protocols as some share ports
        for (unsigned i = protocol->portOffset;
             i < protocol->portOffset + protocol->portCount && i < capRegs->MaxPorts() + 1U; i++) {
            xhci_port_regs_t& port = portRegs[i - 1];
            if (!port.Powered()) {
                port.PowerOn();

                Timer::Wait(20); // 20ms delay

                if (!port.Powered()) {
                    continue; // Not powered so move on
                }
            }

            if (protocol->majorRevision == 0x3) { // USB 3.0
                port.WarmReset();
            } else {
                port.Reset();
            }

            if (!port.Enabled()) {
                int timeout = 25;
                while (timeout-- && !(port.portSC & XHCI_PORTSC_PRC)) {
                    Timer::Wait(1); // Wait for port reset change
                }
            }

            if (port.Enabled()) {
                if (debugLevelXHCI >= DebugLevelVerbose) {
                    Log::Info("Port %i is enabled!", i);
                }
            }
        }
    }
}
} // namespace USB
