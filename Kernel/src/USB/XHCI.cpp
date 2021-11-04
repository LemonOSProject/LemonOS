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

    PCI::EnumerateGenericPCIDevices(xhciClassCode, xhciSubclass, [](const PCIInfo& dev) -> void {
        if (dev.progIf == xhciProgIF) {
            xhciControllers.add_back(new XHCIController(dev));
        }
    });
    return 0;
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

    assert((opRegs->usbStatus & (USB_STS_CNR | USB_STS_HCH)) == USB_STS_HCH);

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

    controllerIRQ = AllocateVector(PCIVectorAny);
    if (controllerIRQ == 0xFF) {
        Log::Error("[XHCI] Failed to allocate IRQ!");
        return;
    }

    IDT::RegisterInterruptHandler(controllerIRQ, reinterpret_cast<isr_t>(&XHCIIRQHandler), this);

    commandRing.ring = reinterpret_cast<xhci_trb_t*>(Memory::KernelAllocate4KPages(1));
    commandRing.physicalAddr = Memory::AllocatePhysicalMemoryBlock();
    Memory::KernelMapVirtualMemory4K(commandRing.physicalAddr, reinterpret_cast<uintptr_t>(commandRing.ring), 1,
                                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

    commandRing.maxIndex = PAGE_SIZE_4K / XHCI_TRB_SIZE;
    memset(commandRing.ring, 0, PAGE_SIZE_4K);

    // Initialize an array of completion events
    commandRing.events = new CommandCompletionEvent[commandRing.maxIndex];

    opRegs->cmdRingCtl = 0; // Clear everything
    opRegs->cmdRingCtl = commandRing.physicalAddr | 1;

    maxSlots = 40;
    if (maxSlots > capRegs->MaxSlots()) {
        maxSlots = capRegs->MaxSlots();
    }
    
    opRegs->SetMaxSlotsEnabled(maxSlots);

    ports = new Port[capRegs->MaxPorts()];
    
    eventRingSegmentTablePhys = Memory::AllocatePhysicalMemoryBlock();
    eventRingSegmentTable = reinterpret_cast<xhci_event_ring_segment_table_entry_t*>(Memory::KernelAllocate4KPages(1));
    Memory::KernelMapVirtualMemory4K(eventRingSegmentTablePhys, reinterpret_cast<uintptr_t>(eventRingSegmentTable), 1,
                                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);
    memset(eventRingSegmentTable, 0, PAGE_SIZE_4K);

    eventRingSegmentTableSize =
        1; // For now we support one segment // PAGE_SIZE_4K / XHCI_EVENT_RING_SEGMENT_TABLE_ENTRY_SIZE;

    int erstMax = 1 << capRegs->ERSTMax();
    assert(eventRingSegmentTableSize <= erstMax);

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
    
    eventRingDequeue = 0;
    eventRingCycleState = true;

    assert(!(opRegs->usbStatus & (USB_STS_HCE)));

    interrupter = &runtimeRegs->interrupters[0];
    interrupter->intModerationInterval = 0; // Disable interrupt moderation
    // Setting ERDP busy clears it
    interrupter->eventRingSegmentTableSize = eventRingSegmentTableSize;
    interrupter->eventRingDequeuePointer = eventRingSegments[0].segmentPhys | XHCI_INT_ERDP_BUSY;
    interrupter->eventRingSegmentTableBaseAddress = eventRingSegmentTablePhys;
    interrupter->interruptEnable = 1;
    interrupter->interruptPending = 1;

    assert(!(opRegs->usbStatus & (USB_STS_HCE)));

    Log::Info("[XHCI] Interface version: %x, Page size: %d, Operational registers offset: %x, Runtime registers "
              "offset: %x, Doorbell registers offset: %x, Supported ERST entries: %d",
              capRegs->hciVersion, opRegs->pageSize, capRegs->capLength, capRegs->rtsOff, capRegs->dbOff, erstMax);

    if (debugLevelXHCI >= DebugLevelNormal) {
        Log::Info("[XHCI] MaxSlots: %x, Max Scratchpad Buffers: %x", maxSlots,
                  capRegs->MaxScratchpadBuffers());
    }
    
    InitializeProtocols();

    opRegs->usbStatus = USB_STS_PCD | USB_STS_EINT | USB_STS_HSE;

    assert(!(opRegs->usbCommand & USB_CMD_RS));
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

    interrupter->interruptEnable = 1;
    interrupter->interruptPending = 1;
    opRegs->usbStatus |= USB_STS_EINT;

    assert(!(opRegs->usbStatus & (USB_STS_HCH)));
    assert(!(opRegs->usbStatus & (USB_STS_CNR)));
    assert(!(opRegs->usbStatus & (USB_STS_HSE)));
    assert(!(opRegs->usbStatus & (USB_STS_HCE)));

    xhci_noop_command_trb_t testCommand;
    memset(&testCommand, 0, sizeof(xhci_noop_command_trb_t));
    testCommand.trbType = TRBTypes::TRBTypeNoOpCommand;
    testCommand.cycleBit = 1;
    commandRing.SendCommandRaw(&testCommand);

    doorbellRegs[0].target = 0;
    //InitializePorts();
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
        xhci_link_trb_t lnkTrb;

        memset(&lnkTrb, 0, sizeof(xhci_link_trb_t));

        if (cycleState)
            lnkTrb.cycleBit = 1;
        else
            lnkTrb.cycleBit = 0;

        lnkTrb.interrupterTarget = 0;
        lnkTrb.interruptOnCompletion = 1;
        lnkTrb.segmentPtr = physicalAddr;
        lnkTrb.toggleCycle = 1;

        ring[enqueueIndex] = *reinterpret_cast<xhci_trb_t*>(&lnkTrb);
        enqueueIndex = 0;

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

        if (enqueueIndex >= maxIndex - 1) {
            xhci_link_trb_t lnkTrb;

            memset(&lnkTrb, 0, sizeof(xhci_link_trb_t));

            if (cycleState)
                lnkTrb.cycleBit = 1;
            else
                lnkTrb.cycleBit = 0;

            lnkTrb.interrupterTarget = 0;
            lnkTrb.interruptOnCompletion = 0;
            lnkTrb.segmentPtr = physicalAddr;
            lnkTrb.toggleCycle = 1;

            ring[enqueueIndex] = *reinterpret_cast<xhci_trb_t*>(&lnkTrb);
            enqueueIndex = 0;

            cycleState = !cycleState;
        }
    }

    hcd->doorbellRegs[0].target = 0;

    bool wasInterrupted = ev->completion.Wait();
    assert(!wasInterrupted);

    return ev->event;
}

void XHCIController::EnableSlot() {
    xhci_enable_slot_command_trb_t trb;
    memset(&trb, 0, XHCI_TRB_SIZE);

    trb.trbType = TRBTypes::TRBTypeEnableSlotCommand;

    commandRing.SendCommandRaw(&trb);
    doorbellRegs[0].target = 0;
}

void XHCIController::OnInterrupt() {
    Log::Info("[XHCI] Interrupt!");

    if(!(opRegs->usbStatus & USB_STS_HCH)){
        Log::Debug(debugLevelXHCI, DebugLevelNormal, "[XHCI] OnInterrupt: Controller Halted");
    }

    if(!(opRegs->usbStatus & USB_STS_EINT)) {
        return;
    }

    opRegs->usbStatus |= USB_STS_EINT;

    if(interrupter->interruptPending){
        interrupter->interruptPending = 1;
    }

    if(!(interrupter->eventRingDequeuePointer & XHCI_INT_ERDP_BUSY)){
        return; // No events
    }

    XHCIEventRingSegment* segment = &eventRingSegments[0];
    xhci_event_trb_t* event = &segment->segment[eventRingDequeue];

    if (debugLevelXHCI >= DebugLevelVerbose) {
        Log::Info("cycle: %Y event cycle: %Y", event->cycleBit, eventRingCycleState);
        for (int i = 0; i < segment->size; i++) {
            Log::Info("%d: cycle: %Y event cycle: %Y (TRB type: %x)", i, segment->segment[i].cycleBit, eventRingCycleState, segment->segment[i].trbType);
        }
    }

    while (!!event->cycleBit == !!eventRingCycleState) {
        if (debugLevelXHCI >= DebugLevelVerbose) {
            Log::Info("[XHCI] Received event (TRB Type: %x (%x))", event->trbType,
                      (((uint32_t*)event)[3] >> 10) & 0x3f);
        }

        if (event->trbType == TRBTypeCommandCompletionEvent) {
            ScopedSpinLock lockRing(commandRing.lock);
            xhci_command_completion_event_trb_t* ccEvent =
                reinterpret_cast<xhci_command_completion_event_trb_t*>(event);

            unsigned cmdIndex = (ccEvent->commandTRBPointer - commandRing.physicalAddr) / XHCI_TRB_SIZE;
            assert(cmdIndex < commandRing.maxIndex);

            auto& ev = commandRing.events[cmdIndex];
            ev.event = *ccEvent;
            ev.completion.Signal();
        }

        event++;
        eventRingDequeue++;
        if (eventRingDequeue &&
            !(eventRingDequeue % segment->size)) { // Check if we are at either beginning or end of ring segment
            eventRingDequeue = 0;
            event = segment->segment;
            eventRingCycleState = !eventRingCycleState;
            break;
        }
    }

    uintptr_t newPtr = reinterpret_cast<uintptr_t>(segment->segmentPhys) + eventRingDequeue * XHCI_TRB_SIZE;

    // Disable XHCI_INT_ERDP_BUSY
    interrupter->eventRingDequeuePointer = (newPtr & ~64ULL) | XHCI_INT_ERDP_BUSY;
    interrupter->interruptPending = 0;
    assert(interrupter->interruptEnable);
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
    auto initializeProto = [&](xhci_ext_cap_supported_protocol_t* ptr) {
        xhci_ext_cap_supported_protocol_t cap = *ptr;

        if (debugLevelXHCI) {
            Log::Info("[XHCI] Initializing protocol \"%c%c%c%c\", Version: %hu%hu.%hu%hu, Port Range: %u-%u",
                      cap.name[0], cap.name[1], cap.name[2], cap.name[3], (cap.majorRevision >> 4),
                      (cap.majorRevision & 0xF), (cap.minorRevision >> 4), (cap.minorRevision & 0xF), cap.portOffset,
                      (cap.portOffset + cap.portCount));
        }

        protocols.add_back(ptr);

        for (unsigned i = cap.portOffset; i < cap.portOffset + cap.portCount && i < capRegs->MaxPorts(); i++) {
            ports[i].protocol = ptr;
        }
    };

    xhci_ext_cap_supported_protocol_t* cap = reinterpret_cast<xhci_ext_cap_supported_protocol_t*>(extCapabilities);
    for (;;) {
        if (cap->capID == XHCIExtendedCapabilities::XHCIExtCapSupportedProtocol) {
            initializeProto(cap);
        } else {
            Log::Debug(debugLevelXHCI, DebugLevelVerbose, "[XHCI] Found cap: %d (%x)", cap->capID, cap->capID);
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

                Device* dev = new Device(this);
                dev->port = i;
                dev->AllocateSlot();
            }
        }
    }
}

void XHCIController::Device::AllocateSlot() {
    assert(port > 0);
    assert(slot == -1);

    xhci_enable_slot_command_trb_t cmd = {};
    cmd.trbType = TRBTypeEnableSlotCommand;
    cmd.slotType = 0;

    auto result = c->commandRing.SendCommand(&cmd);

    auto* cc = reinterpret_cast<xhci_command_completion_event_trb_t*>(&result);
    Log::Info("[XHCI] Allocated slot: %d", (int)cc->slotID);
}

} // namespace USB
