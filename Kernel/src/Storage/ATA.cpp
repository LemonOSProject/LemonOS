#include <Storage/ATA.h>
#include <Storage/ATADrive.h>

#include <APIC.h>
#include <Device.h>
#include <IDT.h>
#include <IOPorts.h>
#include <Logging.h>
#include <Memory.h>
#include <PCI.h>
#include <PhysicalAllocator.h>
#include <Timer.h>
#include <stdint.h>

#include <Debug.h>

namespace ATA {

int port0 = 0x1f0;
int port1 = 0x170;
int controlPort0 = 0x3f6;
int controlPort1 = 0x376;

lock_t irqWatcherLock = 0;
Semaphore irqWatcher = Semaphore(0);

ATADiskDevice* drives[4];

uint32_t busMasterPort;

PCIDevice* controllerPCIDevice;

uint8_t ideClassCode = PCI_CLASS_STORAGE;
uint8_t ideSubclass = PCI_SUBCLASS_IDE;

inline void WriteRegister(uint8_t port, uint8_t reg, uint8_t value) { outportb((port ? port1 : port0) + reg, value); }

inline uint8_t ReadRegister(uint8_t port, uint8_t reg) { return inportb((port ? port1 : port0) + reg); }

inline void WriteControlRegister(uint8_t port, uint8_t reg, uint8_t value) {
    outportb((port ? controlPort1 : controlPort0) + reg, value);
}

inline uint8_t ReadControlRegister(uint8_t port, uint8_t reg) {
    return inportb((port ? controlPort1 : controlPort0) + reg);
}

int DetectDrive(int port, int drive) {
    WriteRegister(port, ATA_REGISTER_DRIVE_HEAD, 0xa0 | (drive << 4)); // Drive

    for (int i = 0; i < 4; i++)
        inportb(controlPort0);

    WriteRegister(port, ATA_REGISTER_SECTOR_COUNT, 0);
    WriteRegister(port, ATA_REGISTER_LBA_LOW, 0);
    WriteRegister(port, ATA_REGISTER_LBA_MID, 0);
    WriteRegister(port, ATA_REGISTER_LBA_HIGH, 0);

    WriteRegister(port, ATA_REGISTER_COMMAND, ATA_CMD_IDENTIFY); // Identify

    Timer::Wait(1);

    if (!ReadRegister(port, ATA_REGISTER_STATUS)) {
        return 0;
    }

    {
        int timer = 0xFFFFFF;
        while (timer--) {
            uint8_t status = ReadRegister(port, ATA_REGISTER_STATUS);
            if (status & ATA_DEV_ERR) {
                return 0;
            }
            if (!(status & ATA_DEV_BUSY) && status & ATA_DEV_DRQ)
                goto c1;
        }
        return 0;
    }

c1:
    if (ReadRegister(port, ATA_REGISTER_LBA_MID) || ReadRegister(port, ATA_REGISTER_LBA_HIGH))
        return 0;

    int timer2 = 0xFFFFFF;
    while (timer2--) {
        uint8_t status = ReadRegister(port, ATA_REGISTER_STATUS);
        if ((status & ATA_DEV_ERR))
            return 0;
        if ((status & ATA_DEV_DRQ))
            return 1;
    }
    return 0;
}

void IRQHandler(RegisterContext* r) {
    Log::Info("Disk IRQ");
    // irqWatcher.Signal();
}

int Init() {
    if (!PCI::FindGenericDevice(ideClassCode, ideSubclass)) {
        Log::Warning("[ATA] No IDE controller found!");
        return 1;
    }

    const PCIInfo& dInfo = PCI::GetGenericPCIDevice(ideClassCode, ideSubclass);
    controllerPCIDevice = new PCIDevice(dInfo);
    assert(controllerPCIDevice->VendorID() != 0xFFFF);

    Log::Info("[ATA] Initializing...");

    if (controllerPCIDevice->BarIsIOPort(0)) {
        uintptr_t bar;
        if ((bar = controllerPCIDevice->GetBaseAddressRegister(0))) {
            port0 = bar;
        }
    }

    if (controllerPCIDevice->BarIsIOPort(1)) {
        uintptr_t bar;
        if ((bar = controllerPCIDevice->GetBaseAddressRegister(1))) {
            port1 = bar;
        }
    }

    if (controllerPCIDevice->BarIsIOPort(2)) {
        uintptr_t bar;
        if ((bar = controllerPCIDevice->GetBaseAddressRegister(2))) {
            controlPort0 = bar;
        }
    }

    if (controllerPCIDevice->BarIsIOPort(3)) {
        uintptr_t bar;
        if ((bar = controllerPCIDevice->GetBaseAddressRegister(3))) {
            controlPort1 = bar;
        }
    }

    assert(controllerPCIDevice->BarIsIOPort(4));
    busMasterPort = controllerPCIDevice->GetBaseAddressRegister(4);
    controllerPCIDevice->EnableBusMastering();

    if (debugLevelATA >= DebugLevelNormal) {
        Log::Info("[ATA] Using Ports: Primary %x, Secondary %x", port0, port1);
    }

    for (int i = 0; i < 2; i++) {                                  // Port
        WriteControlRegister(i, 0, ReadControlRegister(i, 0) | 4); // Software Reset

        for (int z = 0; z < 4; z++)
            inportb(controlPort0);

        WriteControlRegister(i, 0, 0);

        for (int j = 0; j < 2; j++) { // Drive (master/slave)
            if (DetectDrive(i, j)) {
                if (debugLevelATA >= DebugLevelNormal) {
                    Log::Info("Found ATA Drive: Port: %d, %s", i, j ? "slave" : "master");
                }

                for (int k = 0; k < 256; k++) {
                    inportw((i ? port1 : port0) + ATA_REGISTER_DATA);
                }

                drives[i * 2 + j] = new ATADiskDevice(i, j);
            }
        }
    }

    return 0;
}

int Access(ATADiskDevice* drive, uint64_t lba, uint16_t count, bool write) {
    if (count > 1) {
        Log::Warning("ATA::Read was called with count > 1");
        return 1;
    }

    outportb(busMasterPort + ATA_BMR_CMD, 0);
    outportd(busMasterPort + ATA_BMR_PRDT_ADDRESS, drive->prdtPhys);

    outportb(busMasterPort + ATA_BMR_STATUS,
             inportb(busMasterPort + ATA_BMR_STATUS) | 4 | 2); // Clear Error and Interrupt Bits

    WriteRegister(drive->port, ATA_REGISTER_DRIVE_HEAD, 0x40 | (drive->drive << 4));

    for (int i = 0; i < 4; i++)
        inportb(controlPort0);

    while (ReadRegister(drive->port, ATA_REGISTER_STATUS) & ATA_DEV_BUSY)
        ;

    WriteRegister(drive->port, ATA_REGISTER_SECTOR_COUNT, (count >> 8) & 0xFF);

    WriteRegister(drive->port, ATA_REGISTER_LBA_LOW, (lba >> 24) & 0xFF);
    WriteRegister(drive->port, ATA_REGISTER_LBA_MID, (lba >> 32) & 0xFF);
    WriteRegister(drive->port, ATA_REGISTER_LBA_HIGH, (lba >> 40) & 0xFF);

    for (int i = 0; i < 4; i++)
        inportb(controlPort0);

    WriteRegister(drive->port, ATA_REGISTER_SECTOR_COUNT, count & 0xFF);

    WriteRegister(drive->port, ATA_REGISTER_LBA_LOW, lba & 0xFF);
    WriteRegister(drive->port, ATA_REGISTER_LBA_MID, (lba >> 8) & 0xFF);
    WriteRegister(drive->port, ATA_REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);

    for (int i = 0; i < 4; i++)
        inportb(controlPort0);

    while (ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80 ||
           !(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x40))
        ;

    if (write) {
        WriteRegister(drive->port, ATA_REGISTER_COMMAND, 0x35); // 48-bit write DMA
    } else {
        WriteRegister(drive->port, ATA_REGISTER_COMMAND, 0x25); // 48-bit read DMA
    }

    if (write) {
        outportb(busMasterPort + ATA_BMR_CMD, 0 /*Write*/ | 1 /*Start*/);
    } else {
        outportb(busMasterPort + ATA_BMR_CMD, 8 /*Read*/ | 1 /*Start*/);
    }

    uint8_t status = ReadRegister(drive->port, ATA_REGISTER_STATUS);
    while (status & ATA_DEV_BUSY) {
        if (status & ATA_DEV_ERR) {
            break;
        }

        status = ReadRegister(drive->port, ATA_REGISTER_STATUS);
    }

    outportb(busMasterPort + ATA_BMR_CMD, 0);

    if (ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x1) {
        Log::Warning("Disk Error %x", ReadRegister(drive->port, ATA_REGISTER_ERROR));
        return 1;
    }

    return 0;
}
} // namespace ATA