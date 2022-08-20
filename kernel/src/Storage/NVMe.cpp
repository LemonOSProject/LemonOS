#include <Storage/NVMe.h>

#include <Debug.h>
#include <Logging.h>
#include <Math.h>
#include <PCI.h>
#include <Scheduler.h>

namespace NVMe {
char* deviceName = "Generic NVMe Controller";
Vector<Controller*> nvmControllers;

void Initialize() {
    PCI::EnumerateGenericPCIDevices(PCI_CLASS_STORAGE, PCI_SUBCLASS_NVM,
                                    [](const PCIInfo& dev) -> void { nvmControllers.add_back(new Controller(dev)); });
}

NVMeQueue::NVMeQueue(uint16_t qid, uintptr_t cqBase, uintptr_t sqBase, void* cq, void* sq, uint32_t* cqDB,
                     uint32_t* sqDB, uint16_t csz, uint16_t ssz) {
    queueID = qid;

    completionBase = cqBase;
    submissionBase = sqBase;

    completionQueue = reinterpret_cast<NVMeCompletion*>(cq);
    submissionQueue = reinterpret_cast<NVMeCommand*>(sq);

    memset(completionQueue, 0, csz);
    memset(submissionQueue, 0, ssz);

    completionDB = cqDB;
    submissionDB = sqDB;

    cQueueSize = csz;
    cqCount = csz / sizeof(NVMeCompletion);
    sQueueSize = ssz;
    sqCount = ssz / sizeof(NVMeCommand);
}

long NVMeQueue::Consume(NVMeCommand& cmd) { return 0; }

void NVMeQueue::Submit(NVMeCommand& cmd) {
    acquireLock(&queueLock);
    cmd.commandID = nextCommandID++;
    if (nextCommandID == 0xffff) {
        nextCommandID = 0;
    }

    submissionQueue[sqTail] = cmd;

    sqTail++;
    if (sqTail >= sqCount) {
        sqTail = 0;
    }

    *submissionDB = sqTail;
    releaseLock(&queueLock);
}

void NVMeQueue::SubmitWait(NVMeCommand& cmd, NVMeCompletion& complet) {
    ScopedSpinLock lockQueue(queueLock);
    cmd.commandID = nextCommandID++;
    if (nextCommandID == 0xffff) {
        nextCommandID = 0;
    }

    submissionQueue[sqTail] = cmd;

    sqTail++;
    if (sqTail >= sqCount) {
        sqTail = 0;
    }

    *submissionDB = sqTail;

    timeval tv = Timer::GetSystemUptimeStruct();
    while (completionCycleState == !completionQueue[cqHead].phaseTag) {
        if (Timer::TimeDifference(Timer::GetSystemUptimeStruct(), tv) >= 500000) {
            complet.status = 32767;
            return; // Timeout
        }
        Scheduler::Yield();
    }
    complet = completionQueue[cqHead];

    if (++cqHead >= cqCount) {
        cqHead = 0;
        completionCycleState = !completionCycleState;
    }

    *completionDB = cqHead;
}

Controller::Controller(const PCIInfo& dev) : PCIDevice(dev) {

    cRegs = reinterpret_cast<Registers*>(Memory::KernelAllocate4KPages(4));
    Memory::KernelMapVirtualMemory4K(GetBaseAddressRegister(0), (uintptr_t)cRegs, 4,
                                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED | PAGE_WRITETHROUGH);

    EnableBusMastering();
    EnableMemorySpace();
    // pciDevice->EnableInterrupts();

    Log::Info("[NVMe] Initializing Controller... Version: %d.%d.%d, Maximum Queue Entries Supported: %u",
              cRegs->version >> 16, cRegs->version >> 8 & 0xff, cRegs->version & 0xff, GetMaxQueueEntries());

    Disable();

    {
        unsigned spin = 500;
        while (spin-- && (cRegs->status & NVME_CSTS_READY))
            Timer::Wait(1);

        if (spin <= 0) {
            Log::Warning("[NVMe] Controller not disabled! (NVME_CSTS_READY == 1)");
            dStatus = DriverStatus::ControllerError;
            return;
        }
    }

    // memset(cRegs, 0, sizeof(Registers));

    if (GetMinMemoryPageSize() > PAGE_SIZE_4K || GetMaxMemoryPageSize() < PAGE_SIZE_4K) {
        Log::Error("[NVMe] Error: Controller does not support 4K pages");
        return;
    }

    SetMemoryPageSize(PAGE_SIZE_4K);
    SetCommandSet(NVMeConfigCmdSetNVM);

    cRegs->config |= NVME_CFG_DEFAULT_IOCQES | NVME_CFG_DEFAULT_IOSQES;

    uintptr_t admCQBase = Memory::AllocatePhysicalMemoryBlock();
    uintptr_t admSQBase = Memory::AllocatePhysicalMemoryBlock();
    void* admCQ = Memory::KernelAllocate4KPages(1);
    void* admSQ = Memory::KernelAllocate4KPages(1);

    Memory::KernelMapVirtualMemory4K(admCQBase, (uintptr_t)admCQ, 1);
    Memory::KernelMapVirtualMemory4K(admSQBase, (uintptr_t)admSQ, 1);
    memset(admCQ, 0, PAGE_SIZE_4K);
    memset(admSQ, 0, PAGE_SIZE_4K);

    cRegs->adminCompletionQ = admCQBase;
    cRegs->adminSubmissionQ = admSQBase;

    adminQueue = NVMeQueue(0 /* admin queue ID is 0 */, admCQBase, admSQBase, admCQ, admSQ, GetCompletionDoorbell(0),
                           GetSubmissionDoorbell(0), MIN(PAGE_SIZE_4K, GetMaxQueueEntries() * sizeof(NVMeCompletion)),
                           MIN(PAGE_SIZE_4K, GetMaxQueueEntries() * sizeof(NVMeCommand)));

    cRegs->adminQAttr = 0;
    SetAdminCompletionQueueSize(adminQueue.CQSize());
    SetAdminSubmissionQueueSize(adminQueue.SQSize());

    IF_DEBUG(debugLevelNVMe >= DebugLevelVerbose, {
        Log::Info("[NVMe] Admin completion queue: %x (%x), submission queue: %x (%x)", admCQ, admCQBase, admSQ,
                  admSQBase);
        Log::Info("[NVMe] CQ size: %d, SQ size: %d", (cRegs->adminQAttr >> 16) + 1, (cRegs->adminQAttr & 0xffff) + 1);
    });

    Enable();

    {
        unsigned spin = 500;
        while (spin-- && !(cRegs->status & NVME_CSTS_READY))
            Timer::Wait(1);

        if (spin <= 0) {
            Log::Warning("[NVMe] Controller not ready! (NVME_CSTS_READY != 1)");
            dStatus = DriverStatus::ControllerError;
            return;
        }
    }

    if (cRegs->status & NVME_CSTS_FATAL) {
        Log::Warning("[NVMe] Controller fatal error! (NVME_CSTS_FATAL set)");
        dStatus = DriverStatus::ControllerError;
        return;
    }

    if (IdentifyController()) {
        Log::Warning("[NVMe] Failed to identify controller!");
        dStatus = DriverStatus::ControllerError;
        return;
    }

    // Check for one of two things:
    //      1. Controller type is set to I/O Controller
    //      2. Controller type is set to not reported (0) and has >0 namespaces
    if (controllerIdentity->controllerType != ControllerType::IOController &&
        (controllerIdentity->controllerType > 0 && !controllerIdentity->numNamespaces)) {
        Log::Warning("[NVMe] Not an I/O Controller (%d)", controllerIdentity->controllerType);
        Disable();
        return;
    }

    // GetNamespaceList();

    // Attempt to allocate 12 I/O queues
    if (SetNumberOfQueues(12)) {
        dStatus = ControllerError; // Failed to create at least one I/O queue
        return;
    }

    // Create 32 I/O Queues
    for (unsigned i = 0; i < MIN(completionQueuesAllocated, submissionQueuesAllocated); i++) {
        NVMeQueue* qPtr = new NVMeQueue();

        if (CreateIOQueue(qPtr)) { // Error creating I/O queue?
            delete qPtr;
            break;
        }

        ioQueues.add_back(qPtr);
    }

    if (ioQueues.get_length() < 1) {
        Log::Warning("[NVMe] Failed to create any I/O queues!");
        dStatus = ControllerError; // Failed to create at least one I/O queue
        return;
    }

    queueAvailability = Semaphore(ioQueues.get_length());

    IF_DEBUG(debugLevelNVMe >= DebugLevelNormal, {
        char serialNumber[21];
        memcpy(serialNumber, controllerIdentity->serialNumber, 20);
        serialNumber[20] = 0;

        char name[41];
        memcpy(name, controllerIdentity->name, 40);
        name[40] = 0;

        Log::Info("[NVMe] Serial number: '%s', Name: '%s', %d namespaces (%d reported), %d I/O queues", serialNumber,
                  name, namespaceIDs.get_length(), controllerIdentity->numNamespaces, ioQueues.get_length());
    });

    dStatus = ControllerReady;

    for (unsigned i = 0; i < controllerIdentity->numNamespaces; i++) {
        NamespaceIdentity* namespaceIdentity = reinterpret_cast<NamespaceIdentity*>(Memory::KernelAllocate4KPages(1));
        uintptr_t namespaceIdentityPhys = Memory::AllocatePhysicalMemoryBlock();
        Memory::KernelMapVirtualMemory4K(namespaceIdentityPhys, reinterpret_cast<uintptr_t>(namespaceIdentity), 1);

        NVMeCommand identifyNs;
        memset(&identifyNs, 0, sizeof(NVMeCommand));

        identifyNs.opcode = AdminCmdIdentify;
        identifyNs.prp1 = namespaceIdentityPhys;

        identifyNs.identify.cns = NVMeIdentifyCommand::CNSNamespace;
        identifyNs.nsID = i + 1;

        NVMeCompletion completion;
        adminQueue.SubmitWait(identifyNs, completion);

        if (completion.status > 0) {
            Memory::FreePhysicalMemoryBlock(namespaceIdentityPhys);
            Memory::KernelFree4KPages(namespaceIdentity, 1);
            continue;
        }

        Log::Info("[NVMe] Found namespace! NSID: %u", i + 1);

        auto* ns = new Namespace(this, i + 1, *namespaceIdentity);
        if(ns->nsStatus == Namespace::NamespaceStatus::Active) {
            namespaces.add_back(ns);
        } else {
            delete ns;
        }

        Memory::FreePhysicalMemoryBlock(namespaceIdentityPhys);
        Memory::KernelFree4KPages(namespaceIdentity, 1);
    }

    for (auto it = namespaces.begin(); it != namespaces.end(); it++) {
        auto& ns = *it;

        if (ns->nsStatus != Namespace::NamespaceStatus::Active) {
            delete ns;
        }
    }
}

long Controller::CreateIOQueue(NVMeQueue* qPtr) {
    uintptr_t sqBase = Memory::AllocatePhysicalMemoryBlock();
    uintptr_t cqBase = Memory::AllocatePhysicalMemoryBlock();
    void* sq = Memory::KernelAllocate4KPages(1);
    void* cq = Memory::KernelAllocate4KPages(1);

    Memory::KernelMapVirtualMemory4K(cqBase, (uintptr_t)cq, 1, PAGE_CACHE_DISABLED | PAGE_WRITABLE | PAGE_PRESENT);
    Memory::KernelMapVirtualMemory4K(sqBase, (uintptr_t)sq, 1, PAGE_CACHE_DISABLED | PAGE_WRITABLE | PAGE_PRESENT);

    uint16_t queueID = AllocateQueueID();

    NVMeCompletion completion;

    *qPtr = NVMeQueue(queueID, cqBase, sqBase, cq, sq, GetCompletionDoorbell(queueID), GetSubmissionDoorbell(queueID),
                      PAGE_SIZE_4K, PAGE_SIZE_4K);

    NVMeCommand createCq;
    memset(&createCq, 0, sizeof(NVMeCommand));
    createCq.opcode = AdminCmdCreateIOCompletionQueue;

    createCq.createIOCQ.contiguous = 1;
    createCq.createIOCQ.queueID = queueID;
    createCq.createIOCQ.queueSize = qPtr->CQSize() - 1;
    createCq.prp1 = cqBase;

    adminQueue.SubmitWait(createCq, completion);

    if (completion.status > 0) {
        Log::Warning("[NVMe] Status %u creating I/O completion queue", completion.status);
        return completion.status;
    }

    NVMeCommand createSq;
    memset(&createSq, 0, sizeof(NVMeCommand));
    createSq.opcode = AdminCmdCreateIOSubmissionQueue;

    createSq.createIOSQ.contiguous = 1;
    createSq.createIOSQ.queueID = queueID;
    createSq.createIOSQ.queueSize = qPtr->SQSize() - 1;
    createSq.createIOSQ.cqID = queueID;
    createSq.prp1 = sqBase;

    adminQueue.SubmitWait(createSq, completion);

    if (completion.status > 0) {
        Log::Warning("[NVMe] Status %u creating I/O submission queue", completion.status);
        return completion.status;
    }

    return 0;
}

long Controller::SetNumberOfQueues(uint16_t num) {
    NVMeCommand cmd;
    memset(&cmd, 0, sizeof(NVMeCommand));

    cmd.opcode = AdminCmdSetFeatures;

    cmd.setFeatures.featureID = NVMeSetFeaturesCommand::FeatureIDNumberOfQueues;
    cmd.setFeatures.dw11 = (static_cast<uint32_t>(num) << 16) |
                           num; // Number of completion queues in high word, Number of submission queues in low word

    NVMeCompletion completion;
    adminQueue.SubmitWait(cmd, completion);

    if (completion.status > 0) {
        Log::Warning("[NVMe] Status %u setting queue count", completion.status);
        return completion.status;
    }

    completionQueuesAllocated = (completion.dw0 >> 16) & 0xffff; // High word
    submissionQueuesAllocated = completion.dw0 & 0xffff;         // Low Word

    return 0;
}

long Controller::IdentifyController() {
    // if(!controllerIdentityPhys){
    controllerIdentityPhys = Memory::AllocatePhysicalMemoryBlock();
    controllerIdentity = reinterpret_cast<ControllerIdentity*>(Memory::KernelAllocate4KPages(1));
    Memory::KernelMapVirtualMemory4K(controllerIdentityPhys, (uintptr_t)controllerIdentity, 1);
    //}

    NVMeCommand identifyCommand;
    memset(&identifyCommand, 0, sizeof(NVMeCommand));

    identifyCommand.opcode = AdminCmdIdentify;
    identifyCommand.prp1 = controllerIdentityPhys;

    identifyCommand.identify.cns = NVMeIdentifyCommand::CNSController;

    NVMeCompletion completion;
    adminQueue.SubmitWait(identifyCommand, completion);

    if (completion.status > 0) {
        IF_DEBUG(debugLevelNVMe >= DebugLevelVerbose,
                 { Log::Warning("[NVMe] Status %d attempting to identify controller!", completion.status); })
        return completion.status;
    }

    return 0;
}

long Controller::GetNamespaceList() {
    uint32_t* namespaceList = reinterpret_cast<uint32_t*>(Memory::KernelAllocate4KPages(1));
    uintptr_t namespaceListPhys = Memory::AllocatePhysicalMemoryBlock();
    Memory::KernelMapVirtualMemory4K(namespaceListPhys, reinterpret_cast<uintptr_t>(namespaceList), 1);

    NVMeCommand identifyNsList;
    memset(&identifyNsList, 0, sizeof(NVMeCommand));

    identifyNsList.opcode = AdminCmdIdentify;
    identifyNsList.prp1 = namespaceListPhys;

    identifyNsList.identify.cns = NVMeIdentifyCommand::CNSNamespaceList;
    identifyNsList.nsID = 0;

    NVMeCompletion completion;
    adminQueue.SubmitWait(identifyNsList, completion);

    if (completion.status > 0) {
        IF_DEBUG(debugLevelNVMe >= DebugLevelVerbose, {
            Log::Warning("[NVMe] Status %d attempting to retrieve controller namespace list!", completion.status);
        })
        return completion.status;
    }

    uint32_t* namespaceListEnd = namespaceList + (PAGE_SIZE_4K / sizeof(uint32_t));
    while (*namespaceList && namespaceList < namespaceListEnd) {
        namespaceIDs.add_back(*namespaceList++);
    }

    Memory::FreePhysicalMemoryBlock(namespaceListPhys);
    Memory::KernelFree4KPages(namespaceList, 1);

    return 0;
}

NVMeQueue* Controller::AcquireIOQueue() {
    if (queueAvailability.Wait()) {
        return nullptr;
    }

    assert(ioQueues.get_length() > 0);
    return ioQueues.remove_at(0);
}

void Controller::ReleaseIOQueue(NVMeQueue* queue) {
    ioQueues.add_back(queue);

    queueAvailability.Signal();
}
} // namespace NVMe