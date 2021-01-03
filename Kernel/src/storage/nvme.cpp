#include <nvme.h>

#include <pci.h>
#include <logging.h>
#include <debug.h>
#include <scheduler.h>
#include <math.h>

namespace NVMe{
    char* deviceName = "Generic NVMe Controller";
    Vector<Controller*> nvmControllers;

    void Initialize(){
        List<PCIDevice*> devices = PCI::GetGenericPCIDevices(PCI_CLASS_STORAGE, PCI_SUBCLASS_NVM);
        for(PCIDevice* dev : devices){
            nvmControllers.add_back(new Controller(dev));
        }
    }

    NVMeQueue::NVMeQueue(uint16_t qid, uintptr_t cqBase, uintptr_t sqBase, void* cq, void* sq, uint32_t* cqDB, uint32_t* sqDB, uint16_t csz, uint16_t ssz){
        queueID = qid;

        completionBase = cqBase;
        submissionBase = sqBase;

        completionQueue = reinterpret_cast<NVMeCompletion*>(cq);
        submissionQueue = reinterpret_cast<NVMeCommand*>(cq);

        completionDB = cqDB;
        submissionDB = sqDB;

        cQueueSize = csz;
        cqCount = csz / sizeof(NVMeCompletion);
        sQueueSize = ssz;
        sqCount = ssz / sizeof(NVMeCommand);
    }

    long NVMeQueue::Consume(NVMeCommand& cmd){
        return 0;
    }

    void NVMeQueue::Submit(NVMeCommand& cmd){
        acquireLock(&queueLock);
        cmd.commandID = nextCommandID++;

        submissionQueue[sqTail] = cmd;

        sqTail++;
        if(sqTail >= sqCount){
            sqTail = 0;
        }

        *submissionDB = sqTail;
        releaseLock(&queueLock);
    }

    void NVMeQueue::SubmitWait(NVMeCommand& cmd, NVMeCompletion& complet){
        acquireLock(&queueLock);
        cmd.commandID = nextCommandID++;

        submissionQueue[sqTail] = cmd;

        sqTail++;
        if(sqTail >= sqCount){
            sqTail = 0;
        }

        *submissionDB = sqTail;

        while(completionCycleState == !completionQueue[cqHead].phaseTag) Scheduler::Yield();
        complet = completionQueue[cqHead];

        if(++cqHead >= cqCount){
            cqHead = 0;
            completionCycleState = !completionCycleState;
        }

        *completionDB = cqHead;
        releaseLock(&queueLock);
    }

    Controller::Controller(PCIDevice* pciDev){
        pciDevice = pciDev;

        cRegs = reinterpret_cast<Registers*>(Memory::KernelAllocate4KPages(4));
        Memory::KernelMapVirtualMemory4K(pciDevice->GetBaseAddressRegister(0), (uintptr_t)cRegs, 4);
        
        pciDevice->EnableBusMastering();
        pciDevice->EnableMemorySpace();
        pciDevice->EnableInterrupts();

        Disable();

        {
            unsigned spin = 500;
            while(spin-- && (cRegs->status & NVME_CSTS_READY)) Timer::Wait(1);

            if(spin <= 0){
                Log::Warning("[NVMe] Controller not disabled! (NVME_CSTS_READY == 1)");
                dStatus = DriverStatus::ControllerError;
                return;
            }
        }

        cRegs->config = NVME_CFG_DEFAULT_IOCQES | NVME_CFG_DEFAULT_IOSQES;

        if(GetMinMemoryPageSize() > PAGE_SIZE_4K || GetMaxMemoryPageSize() < PAGE_SIZE_4K){
            Log::Error("[NVMe] Error: Controller does not support 4K pages");
            return;
        }

        SetMemoryPageSize(PAGE_SIZE_4K);
        SetCommandSet(NVMeConfigCmdSetNVM);

        uintptr_t admCQBase = Memory::AllocatePhysicalMemoryBlock();
        uintptr_t admSQBase = Memory::AllocatePhysicalMemoryBlock();
        void* admCQ = Memory::KernelAllocate4KPages(1);
        void* admSQ = Memory::KernelAllocate4KPages(1);

        cRegs->adminCompletionQ = admCQBase;
        cRegs->adminSubmissionQ = admSQBase;

        Memory::KernelMapVirtualMemory4K(admCQBase, (uintptr_t)admCQ, 1);
        Memory::KernelMapVirtualMemory4K(admSQBase, (uintptr_t)admSQ, 1);
        
        adminQueue = NVMeQueue(0 /* admin queue ID is 0 */, admCQBase, admSQBase, admCQ, admSQ, GetCompletionDoorbell(0), GetSubmissionDoorbell(0), MIN(PAGE_SIZE_4K, GetMaxQueueEntries() * sizeof(NVMeCompletion)), MIN(PAGE_SIZE_4K, GetMaxQueueEntries() * sizeof(NVMeCommand)));

        SetAdminCompletionQueueSize(adminQueue.CQSize());
        SetAdminSubmissionQueueSize(adminQueue.SQSize());

        Enable();

        {
            unsigned spin = 500;
            while(spin-- && !(cRegs->status & NVME_CSTS_READY)) Timer::Wait(1);

            if(spin <= 0){
                Log::Warning("[NVMe] Controller not ready! (NVME_CSTS_READY != 1)");
                dStatus = DriverStatus::ControllerError;
                return;
            }
        }

        if(IdentifyController()){
            Log::Warning("[NVMe] Failed to identify controller!");
            dStatus = DriverStatus::ControllerError;
            return;
        }

        if(controllerIdentity->controllerType != ControllerType::IOController){
            Log::Warning("[NVMe] Not an I/O Controller");
            Disable();
            return;
        }

        IF_DEBUG(debugLevelNVMe >= DebugLevelNormal, {
            char serialNumber[21];
            memcpy(serialNumber, controllerIdentity->serialNumber, 20);
            serialNumber[20] = 0;

            char name[41];
            memcpy(name, controllerIdentity->name, 40);
            name[40] = 0;

            Log::Info("[NVMe] Found Controller, Version: %d.%d.%d, Serial number: %s, Name: %s, %d namespaces", cRegs->version >> 16, cRegs->version >> 8 & 0xff, cRegs->version & 0xff, serialNumber, name, controllerIdentity->numNamespaces);
        });
    }

    long Controller::IdentifyController(){
        //if(!controllerIdentityPhys){
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

        if(completion.status > 0){
            IF_DEBUG(debugLevelNVMe >= DebugLevelVerbose, {
                Log::Warning("[NVMe] Status %d attempting to identify controller", completion.status);
            })
            return completion.status;
        }

        return 0;
    }
}