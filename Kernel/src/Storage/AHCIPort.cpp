#include <Storage/AHCI.h>

#include <Errno.h>
#include <Logging.h>
#include <Paging.h>
#include <PhysicalAllocator.h>
#include <Scheduler.h>
#include <Storage/ATA.h>
#include <Storage/GPT.h>
#include <Timer.h>

#include <Debug.h>

#define HBA_PxIS_TFES (1 << 30)

namespace AHCI {
Port::Port(int num, hba_port_t* portStructure, hba_mem_t* hbaMem) {
    registers = portStructure;

    registers->cmd &= ~HBA_PxCMD_ST;
    registers->cmd &= ~HBA_PxCMD_FRE;

    SetDeviceName("SATA Hard Disk");

    StopCMD(registers);

    uintptr_t phys;

    // Command list entry size = 32
    // Command list entry maxim count = 32
    // Command list maxim size = 32*32 = 1K per port
    phys = Memory::AllocatePhysicalMemoryBlock();
    registers->clb = (uint32_t)(phys & 0xFFFFFFFF);
    registers->clbu = (uint32_t)(phys >> 32);

    // FIS entry size = 256 bytes per port
    phys = Memory::AllocatePhysicalMemoryBlock();
    registers->fb = (uint32_t)(phys & 0xFFFFFFFF);
    registers->fbu = (uint32_t)(phys >> 32);

    // Command list size = 256*32 = 8K per port
    commandList = (hba_cmd_header_t*)Memory::GetIOMapping(static_cast<uintptr_t>(registers->clb));
    memset(commandList, 0, PAGE_SIZE_4K);

    // FIS
    fis = reinterpret_cast<hba_fis_t*>(Memory::GetIOMapping(static_cast<uintptr_t>(registers->fb)));
    memset((void*)(fis), 0, PAGE_SIZE_4K);

    fis->dsfis.fis_type = FIS_TYPE_DMA_SETUP;
    fis->psfis.fis_type = FIS_TYPE_PIO_SETUP;
    fis->rfis.fis_type = FIS_TYPE_REG_D2H;
    fis->sdbfis[0] = FIS_TYPE_DEV_BITS;

    for (int i = 0; i < 8 /*Support for 8 command slots*/; i++) {
        commandList[i].prdtl = 1;

        phys = Memory::AllocatePhysicalMemoryBlock();
        commandList[i].ctba = (uint32_t)(phys & 0xFFFFFFFF);
        commandList[i].ctbau = (uint32_t)(phys >> 32);

        commandTables[i] = (hba_cmd_tbl_t*)Memory::GetIOMapping(phys);
        memset(commandTables[i], 0, PAGE_SIZE_4K);
    }

    registers->sctl |= (SCTL_PORT_IPM_NOPART | SCTL_PORT_IPM_NOSLUM | SCTL_PORT_IPM_NODSLP);

    if (hbaMem->cap & AHCI_CAP_SALP) {
        registers->cmd &= ~HBA_PxCMD_ASP; // Disable aggressive slumber and partial
    }

    registers->is = 0; // Clear interrupts
    registers->ie = 1;
    registers->fbs &= ~(0xFFFFF000U);

    registers->cmd |= HBA_PxCMD_POD;
    registers->cmd |= HBA_PxCMD_SUD;

    Timer::Wait(10);

    {
        int spin = 100;
        while (spin-- && (registers->ssts & HBA_PxSSTS_DET) != HBA_PxSSTS_DET_PRESENT) {
            Timer::Wait(1);
        }

        if ((registers->ssts & HBA_PxSSTS_DET) != HBA_PxSSTS_DET_PRESENT) {
            Log::Warning("[AHCI] Device not present (DET: %x)", registers->ssts & HBA_PxSSTS_DET);
            status = AHCIStatus::Error;
            return;
        }
    }

    registers->cmd = (registers->cmd & ~HBA_PxCMD_ICC) | HBA_PxCMD_ICC_ACTIVE;

    {
        int spin = 1000;
        while (spin-- && (registers->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ))) {
            Timer::Wait(1);
        }

        if (spin <= 0) {
            if (debugLevelAHCI >= DebugLevelNormal) {
                Log::Info("[AHCI] Port hung" /*, attempting COMRESET"*/);
            }

            registers->sctl = SCTL_PORT_DET_INIT | SCTL_PORT_IPM_NOPART | SCTL_PORT_IPM_NOSLUM |
                              SCTL_PORT_IPM_NODSLP; // Reset the port
        }

        Timer::Wait(10);

        registers->sctl &= ~HBA_PxSSTS_DET; // Disable init mode

        Timer::Wait(10);

        spin = 200;
        while (spin-- && (registers->ssts & HBA_PxSSTS_DET_PRESENT) != HBA_PxSSTS_DET_PRESENT) {
            Timer::Wait(1);
        }

        if ((registers->tfd & 0xff) == 0xff) {
            Timer::Wait(500);
        }

        registers->serr = 0;
        registers->is = 0;

        spin = 1000;
        while (spin-- && registers->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) {
            Timer::Wait(1);
        }

        if (spin <= 0) {
            Log::Info("[AHCI] Port hung!");
        }
    }

    if ((registers->ssts & HBA_PxSSTS_DET) != HBA_PxSSTS_DET_PRESENT) {
        Log::Warning("[AHCI] Device not present (DET: %x)", registers->ssts & HBA_PxSSTS_DET);
        status = AHCIStatus::Error;
        return;
    }

    for (unsigned i = 0; i < 8; i++) {
        physBuffers[i] = Memory::AllocatePhysicalMemoryBlock();
        buffers[i] = Memory::KernelAllocate4KPages(1);
        Memory::KernelMapVirtualMemory4K(physBuffers[i], (uintptr_t)buffers[i], 1);
    }

    status = AHCIStatus::Active;

    Identify();

    if (debugLevelAHCI >= DebugLevelNormal) {
        Log::Info("[AHCI] Port - SSTS: %x, SCTL: %x, SERR: %x, SACT: %x, Cmd/Status: %x, FBS: %x, IE: %x",
                  registers->ssts, registers->sctl, registers->serr, registers->sact, registers->cmd, registers->fbs,
                  registers->ie);
    }

    switch (GPT::Parse(this)) {
    case 0:
        Log::Error("[AHCI] Disk has a corrupted or non-existant GPT. MBR disks are NOT supported.");
        break;
    case -1:
        Log::Error("[AHCI] Disk Error while Parsing GPT for SATA Disk ");
        break;
    }
    Log::Info("[AHCI] Found %d partitions!", partitions.get_length());

    InitializePartitions();

    portLock.SetValue(1);
    bufferSemaphore.SetValue(8);
}

int Port::AcquireBuffer() {
    if (bufferSemaphore.Wait()) {
        return -EINTR;
    }

    int i = 0;
    for (; i < 8; i++) {
        if (!acquireTestLock(&bufferLocks[i])) {
            return i;
        }
    }

    bufferSemaphore.Signal();
    return -1;
}

void Port::ReleaseBuffer(int index) {
    assert(index < 8);

    releaseLock(&bufferLocks[index]);
    bufferSemaphore.Signal();
}

int Port::ReadDiskBlock(uint64_t lba, uint32_t count, void* _buffer) {
    uint64_t blockCount = ((count + (blocksize - 1)) / blocksize);
    uint8_t* buffer = reinterpret_cast<uint8_t*>(_buffer);

    // Log::Info("LBA: %x, count: %u, blcount: %u", lba, count, blockCount);

    int buf = AcquireBuffer();
    if (buf == -EINTR) {
        return EINTR;
    }
    if (buf >= 8 || buf < 0) {
        return 4; // Should not happen
    }

    while (blockCount >= 8 && count) {
        int size = 512 * 8;
        if (size > count)
            size = count;

        if (!size)
            break;

        if (int e = Access(lba, 8, physBuffers[buf], 0); e) { // LBA, 8 blocks, read
            return e;                                         // Error Reading Sectors
        }

        memcpy(buffer, buffers[buf], size);

        buffer += size;
        lba += 8;
        blockCount -= 8;
        count -= size;
    }

    while (blockCount >= 2 && count) {
        int size = 512 << 1;
        if (size > count)
            size = count;

        if (!size)
            break;

        if (int e = Access(lba, 2, physBuffers[buf], 0); e) { // LBA, 2 blocks, read
            ReleaseBuffer(buf);
            return e; // Error Reading Sectors
        }

        memcpy(buffer, buffers[buf], size);

        buffer += size;
        lba += 2;
        blockCount -= 2;
        count -= size;
    }

    while (blockCount && count) {
        int size = 512;
        if (size > count)
            size = count;

        if (!size)
            break;

        if (int e = Access(lba, 1, physBuffers[buf], 0); e) { // LBA, 1 block, read
            ReleaseBuffer(buf);
            return e; // Error Reading Sectors
        }

        memcpy(buffer, buffers[buf], size);

        buffer += size;
        lba++;
        blockCount--;
        count -= size;
    }
    ReleaseBuffer(buf);
    return 0;
}

int Port::WriteDiskBlock(uint64_t lba, uint32_t count, void* _buffer) {
    uint64_t blockCount = ((count + (blocksize - 1)) / blocksize);
    uint8_t* buffer = reinterpret_cast<uint8_t*>(_buffer);

    unsigned buf = AcquireBuffer();
    if (buf >= 8) {
        return 4; // Should not happen
    }

    while (blockCount-- && count) {
        uint64_t size;
        if (count < 512)
            size = count;
        else
            size = 512;

        if (!size)
            break;

        memcpy(buffers[buf], buffer, size);

        if (int e = Access(lba, 1, physBuffers[buf], 1); e) { // LBA, 1 block, write
            ReleaseBuffer(buf);
            return e; // Error Reading Sectors
        }

        buffer += size;
        lba++;
    }
    ReleaseBuffer(buf);

    return 0;
}

int Port::Access(uint64_t lba, uint32_t count, uintptr_t physBuffer, int write) {
    assert(CheckInterrupts());

    if (portLock.Wait()) {
        return -EINTR;
    }

    registers->ie = 0xffffffff;
    registers->is = 0;
    int spin = 0;

    int slot = FindCmdSlot();
    if (slot == -1) {
        Log::Warning("[SATA] Could not find command slot!");

        portLock.Signal();
        return -EIO;
    }

    registers->serr = registers->tfd = 0;

    hba_cmd_header_t* commandHeader = &commandList[slot];

    commandHeader->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);

    commandHeader->a = 0;
    commandHeader->w = write;
    commandHeader->c = 0;
    commandHeader->p = 0;

    commandHeader->prdbc = 0;
    commandHeader->pmp = 0;

    hba_cmd_tbl_t* commandTable = commandTables[slot];
    memset(commandTable, 0, sizeof(hba_cmd_tbl_t));

    commandTable->prdt_entry[0].dba = physBuffer & 0xFFFFFFFF;
    commandTable->prdt_entry[0].dbau = (physBuffer >> 32) & 0xFFFFFFFF;
    commandTable->prdt_entry[0].dbc = 512 * count - 1; // 512 bytes per sector
    commandTable->prdt_entry[0].i = 1;

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(commandTable->cfis);
    memset(commandTable->cfis, 0, sizeof(fis_reg_h2d_t));

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;      // Command
    cmdfis->pmport = 0; // Port multiplier

    if (write) {
        cmdfis->command = ATA_CMD_WRITE_DMA_EX;
    } else {
        cmdfis->command = ATA_CMD_READ_DMA_EX;
    }

    cmdfis->lba0 = lba & 0xFF;
    cmdfis->lba1 = (lba >> 8) & 0xFF;
    cmdfis->lba2 = (lba >> 16) & 0xFF;
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (lba >> 24) & 0xFF;
    cmdfis->lba4 = (lba >> 32) & 0xFF;
    cmdfis->lba5 = (lba >> 40) & 0xFF;

    cmdfis->countl = count & 0xff;
    cmdfis->counth = count >> 8;

    cmdfis->control = 0x8;

    spin = Timer::UsecondsSinceBoot() + 100000;
    while ((registers->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin) {
        if (Timer::UsecondsSinceBoot() >= spin) {
            spin = 0;
        }

        Scheduler::Yield();
    }

    if (spin <= 0) {
        Log::Warning("[SATA] Port Hung");

        portLock.Signal();
        return -EIO;
    }

    registers->ie = registers->is = 0xffffffff;

    StartCMD(registers);
    registers->ci |= 1 << slot;

    // Log::Info("SERR: %x, Slot: %x, PxCMD: %x, Int status: %x, Ci: %x, TFD: %x", registers->serr, slot,
    // registers->cmd, registers->is, registers->ci, registers->tfd);

    spin = Timer::UsecondsSinceBoot() + 200000;
    while ((registers->ci & (1 << slot)) && Timer::UsecondsSinceBoot() < spin) {
        if (registers->is & HBA_PxIS_TFES) // Task file error
        {
            Log::Warning("[SATA] Disk Error (SERR: %x)", registers->serr);

            StopCMD(registers);
            portLock.Signal();
            return -EIO;
        }
    }

    spin = Timer::UsecondsSinceBoot() + 100000;
    while ((registers->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin) {
        if (Timer::UsecondsSinceBoot() >= spin) {
            spin = 0;
        }

        Scheduler::Yield();
    }

    StopCMD(registers);

    if (spin <= 0) {
        Log::Warning("[SATA] Port Hung");

        portLock.Signal();
        return -EIO;
    }

    // Log::Info("SERR: %x, Slot: %x, PxCMD: %x, Int status: %x, Ci: %x, TFD: %x", registers->serr, slot,
    // registers->cmd, registers->is, registers->ci, registers->tfd);

    if (registers->is & HBA_PxIS_TFES) {
        Log::Warning("[SATA] Disk Error (SERR: %x)", registers->serr);

        portLock.Signal();
        return -EIO;
    }

    portLock.Signal();
    return 0;
}

void Port::Identify() {

    registers->ie = 0xffffffff;
    registers->is = 0;
    int spin = 0;

    registers->tfd = 0;

    int slot = FindCmdSlot();
    if (slot == -1) {
        Log::Warning("[SATA] Could not find command slot!");
        return;
    }

    hba_cmd_header_t* commandHeader = &commandList[slot];

    commandHeader->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);

    commandHeader->a = 0;
    commandHeader->w = 0;
    commandHeader->c = 0;
    commandHeader->p = 0;

    commandHeader->prdbc = 0;
    commandHeader->pmp = 0;

    hba_cmd_tbl_t* commandTable = commandTables[slot];
    memset(commandTable, 0, sizeof(hba_cmd_tbl_t));

    commandTable->prdt_entry[0].dba = physBuffers[0] & 0xFFFFFFFF;
    commandTable->prdt_entry[0].dbau = (physBuffers[0] >> 32) & 0xFFFFFFFF;
    commandTable->prdt_entry[0].dbc = 512 - 1; // 512 bytes per sector
    commandTable->prdt_entry[0].i = 1;

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(commandTable->cfis);
    memset(commandTable->cfis, 0, sizeof(fis_reg_h2d_t));

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;      // Command
    cmdfis->pmport = 0; // Port multiplier
    cmdfis->command = ATA_CMD_IDENTIFY;

    cmdfis->lba0 = 0;
    cmdfis->lba1 = 0;
    cmdfis->lba2 = 0;
    cmdfis->device = 0;

    cmdfis->lba3 = 0;
    cmdfis->lba4 = 0;
    cmdfis->lba5 = 0;

    cmdfis->countl = 0;
    cmdfis->counth = 0;

    cmdfis->control = 0;

    spin = 100;
    while ((registers->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin--) {
        Timer::Wait(1);
    }

    if (spin >= 100) {
        Log::Warning("[SATA] Port Hung");
        return;
    }

    registers->ie = registers->is = 0xffffffff;

    StartCMD(registers);
    registers->sact |= 1 << slot;
    registers->ci |= 1 << slot;

    // Log::Info("SERR: %x, Slot: %x, PxCMD: %x, Int status: %x, Ci: %x, TFD: %x", registers->serr, slot,
    // registers->cmd, registers->is, registers->ci, registers->tfd);

    while (registers->ci & (1 << slot)) {
        if (registers->is & HBA_PxIS_TFES) // Task file error
        {
            Log::Warning("[SATA] Disk Error (SERR: %x)", registers->serr);
            StopCMD(registers);
            return;
        }
    }

    StopCMD(registers);

    spin = 100;
    while ((registers->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin--) {
        Timer::Wait(1);
    }

    // Log::Info("SERR: %x, Slot: %x, PxCMD: %x, Int status: %x, Ci: %x, TFD: %x", registers->serr, slot,
    // registers->cmd, registers->is, registers->ci, registers->tfd);

    if (registers->is & HBA_PxIS_TFES) {
        Log::Warning("[SATA] Disk Error (SERR: %x)", registers->serr);
        StopCMD(registers);
        return;
    }
    return;
}

int Port::FindCmdSlot() {
    // If not set in SACT and CI, the slot is free
    uint32_t slots = (registers->sact | registers->ci);
    for (int i = 0; i < 8; i++) {
        if ((slots & 1) == 0)
            return i;
        slots >>= 1;
    }

    return -1;
}

void StopCMD(hba_port_t* port) {
    // Clear ST (bit0)
    port->cmd &= ~HBA_PxCMD_ST;

    int spin = Timer::UsecondsSinceBoot() + 100000;

    // Wait until FR (bit14), CR (bit15) are cleared
    while ((port->cmd & HBA_PxCMD_CR) && spin) {
        if (Timer::UsecondsSinceBoot() >= spin) {
            spin = 0;
        }
    }

    if (spin <= 0) {
        Log::Warning("[SATA] StopCMD Hung");
    }

    // Clear FRE (bit4)
    port->cmd &= ~HBA_PxCMD_FRE;
}

void StartCMD(hba_port_t *port) {
    port->cmd &= ~HBA_PxCMD_ST;

    int spin = Timer::UsecondsSinceBoot() + 100000;

    // Wait until CR (bit15) is cleared
    while ((port->cmd & HBA_PxCMD_CR) && spin) {
        if (Timer::UsecondsSinceBoot() >= spin) {
            spin = 0;
        }
    }

    if (spin <= 0) {
        Log::Warning("[SATA] StartCMD Hung");
    }

    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST; 
}

} // namespace AHCI
