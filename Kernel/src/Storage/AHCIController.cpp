#include <Storage/AHCI.h>

#include <IDT.h>
#include <Logging.h>
#include <Memory.h>
#include <PCI.h>
#include <Paging.h>
#include <Timer.h>

#include <Debug.h>

namespace AHCI {
uintptr_t ahciBaseAddress;
uintptr_t ahciVirtualAddress;
hba_mem_t* ahciHBA;

Port* ports[32];

PCIDevice* controllerPCIDevice;
uint8_t ahciClassCode = PCI_CLASS_STORAGE;
uint8_t ahciSubclass = PCI_SUBCLASS_SATA;

void InterruptHandler(void*, RegisterContext* r) {}

int Init() {
    if (!PCI::FindGenericDevice(ahciClassCode, ahciSubclass)) {
        Log::Warning("[AHCI] No controller found.");
        return 1;
    }

    const PCIInfo& dInfo = PCI::GetGenericPCIDevice(ahciClassCode, ahciSubclass);
    controllerPCIDevice = new PCIDevice(dInfo);

    assert(controllerPCIDevice->VendorID() != 0xFFFF);

    controllerPCIDevice->EnableBusMastering();
    controllerPCIDevice->EnableInterrupts();
    controllerPCIDevice->EnableMemorySpace();

    ahciBaseAddress = controllerPCIDevice->GetBaseAddressRegister(5); // BAR 5 is the AHCI Base Address
    ahciVirtualAddress = Memory::GetIOMapping(ahciBaseAddress);

    ahciHBA = (hba_mem_t*)ahciVirtualAddress;

    /*uint8_t irq = controllerPCIDevice->AllocateVector(PCIVectors::PCIVectorAny);
    if (irq == 0xFF) {
        Log::Warning("[AHCI] Failed to allocate vector!");
    }
    //IDT::RegisterInterruptHandler(irq, InterruptHandler);*/

    uint32_t pi = ahciHBA->pi;

    while (!(ahciHBA->ghc & AHCI_GHC_ENABLE)) {
        ahciHBA->ghc |= AHCI_GHC_ENABLE;
        Timer::Wait(1);
    }
    ahciHBA->ghc &= ~AHCI_GHC_IE;

    if (debugLevelAHCI >= DebugLevelNormal) {
        Log::Info("[AHCI] Interrupt Vector: %x, Base Address: %x, Virtual Base Address: %x", -1, ahciBaseAddress,
                  ahciVirtualAddress);
        Log::Info("[AHCI] (Cap: %x, Cap2: %x) Enabled? %Y, BOHC? %Y, 64-bit addressing? %Y, Staggered Spin-up? %Y, "
                  "Slumber State Capable? %Y, Partial State Capable? %Y, FIS-based switching? %Y",
                  ahciHBA->cap, ahciHBA->cap2, ahciHBA->ghc & AHCI_GHC_ENABLE, ahciHBA->cap2 & AHCI_CAP2_BOHC,
                  ahciHBA->cap & AHCI_CAP_S64A, ahciHBA->cap & AHCI_CAP_SSS, ahciHBA->cap & AHCI_CAP_SSC,
                  ahciHBA->cap & AHCI_CAP_PSC, ahciHBA->cap & AHCI_CAP_FBSS);
    }

    ahciHBA->is = 0xffffffff;

    for (int i = 0; i < 32; i++) {
        if ((pi >> i) & 1) {
            if (((ahciHBA->ports[i].ssts >> 8) & 0x0F) != HBA_PORT_IPM_ACTIVE ||
                (ahciHBA->ports[i].ssts & HBA_PxSSTS_DET) != HBA_PxSSTS_DET_PRESENT)
                continue;

            if (ahciHBA->ports[i].sig == SATA_SIG_ATAPI)
                ;
            else if (ahciHBA->ports[i].sig == SATA_SIG_PM)
                ;
            else if (ahciHBA->ports[i].sig == SATA_SIG_SEMB)
                ;
            else {
                if (debugLevelAHCI >= DebugLevelVerbose) {
                    Log::Info("Found SATA Drive - Port: %d", i);
                }

                ports[i] = new Port(i, &ahciHBA->ports[i], ahciHBA);

                if (ports[i]->status != AHCIStatus::Active) {
                    delete ports[i];
                    ports[i] = nullptr;
                }
            }
        }
    }

    return 0;
}
} // namespace AHCI