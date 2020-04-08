#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <physicalallocator.h>
#include <system.h>
#include <idt.h>
#include <timer.h>

#define INTEL_VENDOR_ID 0x8086

#define I8254_REGISTER_CTRL         0x0
#define I8254_REGISTER_STATUS       0x08
#define I8254_REGISTER_EECD         0x10
#define I8254_REGISTER_EEPROM       0x14
#define I8254_REGISTER_CTRL_EXT     0x18
#define I8254_REGISTER_INT_READ     0xC0


#define I8254_REGISTER_RCTL         0x100
#define I8254_REGISTER_RDESC_LO     0x2800
#define I8254_REGISTER_RDESC_HI     0x2804
#define I8254_REGISTER_RDESC_LEN    0x2808
#define I8254_REGISTER_RDESC_HEAD   0x2810
#define I8254_REGISTER_RDESC_TAIL   0x2818

#define I8254_REGISTER_TCTL         0x400
#define I8254_REGISTER_TDESC_LO     0x3800
#define I8254_REGISTER_TDESC_HI     0x3804
#define I8254_REGISTER_TDESC_LEN    0x3808
#define I8254_REGISTER_TDESC_HEAD   0x3810
#define I8254_REGISTER_TDESC_TAIL   0x3818

namespace Intel8254x{
    uint16_t supportedDevices[]{
        0x1019,
        0x101A,
        0x1010,
        0x1012,
        0x101D,
        0x1079,
        0x107A,
        0x107B,
        0x100F,
        0x1011,
        0x1026,
        0x1027,
        0x1028,
        0x1004,
        0x1107,
        0x1112,
        0x1013,
        0x1018,
        0x1076,
        0x1078,
        0x1017,
        0x1016,
        0x100e,
        0x1015,
    };

    typedef struct {
        uint16_t length; // Length
        uint16_t checksum; // Packet Checksum
        uint8_t status; // Status
        uint8_t errors; // Errors
        uint16_t spec; // Special
    } r_desc_t __attribute__((packed));

    struct {
        uint64_t addr; // Buffer Address
        uint16_t length;
        uint8_t cso; // Checksum Offset
        uint8_t cmd; // Command
        uint8_t status; // Status
        uint8_t reserved;
        uint8_t css;
        uint16_t special;
    } t_desc_t __attribute__((packed));

    int supportedDeviceCount = 23;

    const char* deviceName = "Intel 8254x Compaitble Ethernet Controller";

    pci_device_t device;

    uint64_t memBase;
    void* memBaseVirt;

    bool hasEEPROM = false;

    void WriteMem32(uintptr_t address, uint32_t data){
        *((uint32_t*)(memBaseVirt + address)) = data;
    }

    uint32_t ReadMem32(uintptr_t address){
        return *((uint32_t*)(memBaseVirt + address));
    }

    static uint16_t ReadEEPROM(uint8_t addr) {
        uint32_t temp = 0;
        WriteMem32(I8254_REGISTER_EEPROM, 1 | (((uint32_t)addr) << 8));
        while (!((temp = ReadMem32(I8254_REGISTER_EEPROM)) & (1 << 4)));
        return (uint16_t)((temp >> 16) & 0xFFFF);
    }

    bool CheckForEEPROM(){
        WriteMem32(I8254_REGISTER_EEPROM, 1);

        for (int i = 0; i < 1000; i++) {
            uint32_t reg = ReadMem32(I8254_REGISTER_EEPROM);
            if(reg & 0x10) return true;
        }

        return false;
    }

    void InterruptHandler(regs64_t* r){
        Log::Info("net interrupt");
        uint32_t status = ReadMem32(I8254_REGISTER_INT_READ);

        if(status & 0x04){
            Log::Info("Initializing Link...");
        }
    }

    void Initialize(){
        for(int i = 0; i < supportedDeviceCount; i++){
            if(PCI::FindDevice(supportedDevices[i],INTEL_VENDOR_ID)){
                device.deviceID = supportedDevices[i];
                break;
            }
        }

        device.vendorID = INTEL_VENDOR_ID;
        device.generic = false;
        device.deviceName = deviceName;

        device = PCI::RegisterPCIDevice(device);

        if(device.vendorID == 0xFFFF){
            Log::Warning("No 8254x device found!");
            return;
        }

        memBase = (device.header0.headerType ? device.header1.baseAddress0 : device.header0.baseAddress0) & 0xFFFFFFFFFFFFF000;

        memBaseVirt = Memory::KernelAllocate4KPages(8);
        Memory::KernelMapVirtualMemory4K(memBase,(uintptr_t)memBaseVirt,8);

        Memory::MarkMemoryRegionUsed(memBase, 8*0x1000);

        Log::Info("i8254x    Base Address: ");
        Log::Write(device.header0.baseAddress0);
        Log::Write(", Base Address (virtual): ");
        Log::Write((uintptr_t)memBaseVirt);
        hasEEPROM = CheckForEEPROM();
        Log::Write(", EEPROM Present: ");
        Log::Write(hasEEPROM ? "true" : "false");

        int irqNum = device.header0.interruptLine;
        Log::Write(",IRQ: ");
        Log::Write(irqNum);

        IDT::RegisterInterruptHandler(irqNum, InterruptHandler);

        uint8_t macAddr[6];

        uint16_t cmd = device.header0.command;
        cmd |= (4 | 1); // Enable Bus Mastering

        PCI::Config_WriteWord(device.bus,device.slot,0,4,cmd);

        uint32_t t;
		t = ReadEEPROM(0);
		macAddr[0] = t & 0xFF;
		macAddr[1] = t >> 8;
		t = ReadEEPROM(1);
		macAddr[2] = t & 0xFF;
		macAddr[3] = t >> 8;
		t = ReadEEPROM(2);
		macAddr[4] = t & 0xFF;
		macAddr[5] = t >> 8;

        Log::Write(", MAC Address: ");
        Log::Write(macAddr[0]);
        Log::Write(":");
        Log::Write(macAddr[1]);
        Log::Write(":");
        Log::Write(macAddr[2]);
        Log::Write(":");
        Log::Write(macAddr[3]);
        Log::Write(":");
        Log::Write(macAddr[4]);
        Log::Write(":");
        Log::Write(macAddr[5]);return;

        uint32_t ctrlReg = ReadMem32(I8254_REGISTER_CTRL);
        WriteMem32(I8254_REGISTER_CTRL, ctrlReg | (0x80000000));
        ReadMem32(I8254_REGISTER_STATUS);
        uint64_t _t = Timer::GetSystemUptime();
        while((Timer::GetSystemUptime() - _t) < 2);

        WriteMem32(I8254_REGISTER_CTRL, ctrlReg | (0x4000000));
        ReadMem32(I8254_REGISTER_STATUS);
        _t = Timer::GetSystemUptime();
        while((Timer::GetSystemUptime() - _t) < 2);

        WriteMem32(I8254_REGISTER_CTRL, ctrlReg | (0x00002000));
        ReadMem32(I8254_REGISTER_STATUS);
        _t = Timer::GetSystemUptime();
        while((Timer::GetSystemUptime() - _t) < 2);

        ctrlReg = ReadMem32(I8254_REGISTER_CTRL);
        WriteMem32(I8254_REGISTER_CTRL, ctrlReg | (1 << 26));
        _t = Timer::GetSystemUptime();
        while((Timer::GetSystemUptime() - _t) < 2);

        uint32_t status = ReadMem32(I8254_REGISTER_CTRL);
        status |= (1 << 5);   /* set auto speed detection */
        status |= (1 << 6);   /* set link up */
        status &= ~(1 << 3);  /* unset link reset */
        status &= ~(1UL << 31UL); /* unset phy reset */
        status &= ~(1 << 7);  /* unset invert loss-of-signal */
        WriteMem32(I8254_REGISTER_CTRL, status);


        /* Disables flow control */
        WriteMem32(0x0028, 0);
        WriteMem32(0x002c, 0);
        WriteMem32(0x0030, 0);
        WriteMem32(0x0170, 0);

        /* Unset flow control */
        status = ReadMem32(I8254_REGISTER_CTRL);
        status &= ~(1 << 30);
        WriteMem32(I8254_REGISTER_CTRL, status);

        for (int i = 0; i < 128; ++i) {
            WriteMem32(0x5200 + i * 4, 0);
        }

        for (int i = 0; i < 64; ++i) {
            WriteMem32(0x4000 + i * 4, 0);
        }
    }
}