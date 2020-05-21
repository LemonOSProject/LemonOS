#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <physicalallocator.h>
#include <system.h>
#include <idt.h>
#include <timer.h>
#include <acpi.h>
#include <apic.h>
#include <net/networkadapter.h>

#define INTEL_VENDOR_ID 0x8086

#define I8254_REGISTER_CTRL         0x0
#define I8254_REGISTER_STATUS       0x08
#define I8254_REGISTER_EECD         0x10
#define I8254_REGISTER_EEPROM       0x14
#define I8254_REGISTER_CTRL_EXT     0x18
#define I8254_REGISTER_INT_READ     0xC0
#define I8254_REGISTER_INT_MASK     0xD0

#define I8254_REGISTER_RCTRL        0x100
#define I8254_REGISTER_RDESC_LO     0x2800
#define I8254_REGISTER_RDESC_HI     0x2804
#define I8254_REGISTER_RDESC_LEN    0x2808
#define I8254_REGISTER_RDESC_HEAD   0x2810
#define I8254_REGISTER_RDESC_TAIL   0x2818

#define I8254_REGISTER_TCTRL        0x400
#define I8254_REGISTER_TDESC_LO     0x3800
#define I8254_REGISTER_TDESC_HI     0x3804
#define I8254_REGISTER_TDESC_LEN    0x3808
#define I8254_REGISTER_TDESC_HEAD   0x3810
#define I8254_REGISTER_TDESC_TAIL   0x3818

#define I8254_REGISTER_MTA          0x5200

#define CTRL_FD (1 << 0)        // Full Duplex
#define CTRL_LINK_RESET (1 << 3)
#define CTRL_ASDE (1 << 5)      // Auto speed detection enable
#define CTRL_SLU (1 << 6)       // Set Link Up

#define RCTRL_ENABLE (1 << 1)   // Receiver Enable
#define RCTRL_SBP (1 << 2)      // Store bad packets
#define RCTRL_UPE (1 << 3)      // Unicast Promiscous Enable
#define RCTRL_MPE (1 << 4)      // Multicast Promiscous Enable
#define RCTRL_LPE (1 << 5)      // Long packet reception enbale
#define RCTRL_LBM (3 << 6)      // Loopback - Controls the loopback mode: 00b no loopback, 11b PHY or external serdes loopback
#define RCTRL_RDMTS (3 << 8)    // Receive Descriptor Minimum Threshold Size
#define RCTRL_MO (3 << 12)      // Multicast offset
#define RCTRL_BAM (1 << 15)     // Broadcast accept mode
#define RCTRL_BSIZE (3 << 16)   // Buffer size 00b - 2048/REserved, 01b - 1024/16384 bytes, 10b - 512/8192 bytes, 11b 256/4096
#define RCTRL_G_BSIZE(x) ((x & 3) << 16)
#define RCTRL_BSEX (1 << 25)    // Buffer size extension
#define RCTRL_SECRC (1 << 26)   // Strip Ehternet CRC

#define TCTRL_ENABLE (1 << 1)           // Transmitter Enable
#define TCTRL_PSP (1 << 3)              // Pad Short Packets
#define TCTRL_CT(x) ((x & 0xFF) << 4)   // Collision Threshold
#define TCTRL_COLD(x) ((x & 0xFF) << 12)// Colision Distance

#define BSIZE_4096 (RCTRL_G_BSIZE(3) | RCTRL_BSEX)

#define TCMD_EOP (1 << 0) // End of packet
#define TCMD_IFCS (1 << 1) // Insert FCS
#define TCMD_IC (1 << 2) // Insert Checksum
#define TCMD_RS (1 << 3) // Report Status
#define TCMD_VLE (1 << 6) // VLAN Packet Enable
#define TCMD_IDE (1 << 7) // Interrupt Delay Enable

#define TSTATUS_DD (1 << 0) // Descriptor Done
#define TSTATUS_EC (1 << 1) // Excess Collisions
#define TSTATUS_LC (1 << 2) // Late collision

#define STATUS_LINK_UP (1 << 1)
#define STATUS_SPEED (3 << 6)   // 00b - 10Mb/s, 01b - 100MB/s, 10b/11b - 1000Mb/s

#define SPEED_10 0
#define SPEED_100 1
#define SPEED_1000 2
#define SPEED_1000_ALT 3

#define RX_DESC_COUNT 256
#define TX_DESC_COUNT 256

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
        uint64_t addr; // Buffer Address
        uint16_t length; // Length
        uint16_t checksum; // Packet Checksum
        uint8_t status; // Status
        uint8_t errors; // Errors
        uint16_t special; // Special
    } r_desc_t __attribute__((packed));

    typedef struct {
        uint64_t addr; // Buffer Address
        uint16_t length;
        uint8_t cso; // Checksum Offset
        uint8_t cmd; // Command
        uint8_t status : 4; // Status
        uint8_t reserved : 4; // Reserved
        uint8_t css; // Checksum start field
        uint16_t special; // Special
    } t_desc_t __attribute__((packed));

    int supportedDeviceCount = 23;

    const char* deviceName = "Intel 8254x Compaitble Ethernet Controller";

    pci_device_t device;

    uint64_t memBase;
    void* memBaseVirt;

    bool hasEEPROM = false;

    r_desc_t* rxDescriptors;
    t_desc_t* txDescriptors;

    void WriteMem32(uintptr_t address, uint32_t data){
        *((uint32_t*)(memBaseVirt + address)) = data;
    }

    uint32_t ReadMem32(uintptr_t address){
        return *((uint32_t*)(memBaseVirt + address));
    }

    int GetSpeed(){
        int spd = ReadMem32(I8254_REGISTER_STATUS) & STATUS_SPEED;
        spd >>= 6;

        switch(spd){
        case SPEED_10:
            spd = 10;
            break;
        case SPEED_100:
            spd = 100;
            break;
        case SPEED_1000:
        case SPEED_1000_ALT:
            spd = 1000;
            break;
        default:
            spd = 0;
            break;
        }

        return spd;
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
            Log::Info("[i8254x] Initializing Link...");

            WriteMem32(I8254_REGISTER_CTRL, ReadMem32(I8254_REGISTER_CTRL) | CTRL_SLU | CTRL_ASDE);

            Log::Info("[i8254x] Link %s, Speed: %d", (ReadMem32(I8254_REGISTER_STATUS) | STATUS_LINK_UP) ? "Up" : "Down", GetSpeed());
        }
    }

    void InitializeRx(){
        uint64_t rxDescPhys = Memory::AllocatePhysicalMemoryBlock(); // The card wants a physical address
        rxDescriptors = (r_desc_t*)Memory::GetIOMapping(rxDescPhys);
        uint32_t rxLow = rxDescPhys & 0xFFFFFFFF;
        uint32_t rxHigh = rxDescPhys >> 32;
        uint32_t rxLen = 4096; // Memory block size
        uint32_t rxHead = 0;
        uint32_t rxTail = RX_DESC_COUNT - 1; // Offset from base

        WriteMem32(I8254_REGISTER_RDESC_LO, rxLow);
        WriteMem32(I8254_REGISTER_RDESC_HI, rxHigh);
        WriteMem32(I8254_REGISTER_RDESC_LEN, rxLen);
        WriteMem32(I8254_REGISTER_RDESC_HEAD, rxHead);
        WriteMem32(I8254_REGISTER_RDESC_TAIL, rxTail);

        for(int i = 0; i < RX_DESC_COUNT; i++){
            r_desc_t* rxd = &rxDescriptors[i];
            rxd->addr = Memory::AllocatePhysicalMemoryBlock();
            rxd->status = 0;
        }

        WriteMem32(I8254_REGISTER_TCTRL, (TCTRL_ENABLE | TCTRL_PSP));
    }
    
    void InitializeTx(){
        uint64_t txDescPhys = Memory::AllocatePhysicalMemoryBlock(); // The card wants a physical address
        txDescriptors = (t_desc_t*)Memory::GetIOMapping(txDescPhys);
        uint32_t txLow = txDescPhys & 0xFFFFFFFF;
        uint32_t txHigh = txDescPhys >> 32;
        uint32_t txLen = 4096; // Memory block size
        uint32_t txHead = 0;
        uint32_t txTail = RX_DESC_COUNT - 1; // Offset from base

        WriteMem32(I8254_REGISTER_TDESC_LO, txLow);
        WriteMem32(I8254_REGISTER_TDESC_HI, txHigh);
        WriteMem32(I8254_REGISTER_TDESC_LEN, txLen);
        WriteMem32(I8254_REGISTER_TDESC_HEAD, txHead);
        WriteMem32(I8254_REGISTER_TDESC_TAIL, txTail);

        for(int i = 0; i < TX_DESC_COUNT; i++){
            t_desc_t* txd = &txDescriptors[i];
            txd->addr = Memory::AllocatePhysicalMemoryBlock();
            txd->status = 0;
        }

        WriteMem32(I8254_REGISTER_RCTRL, (RCTRL_ENABLE | RCTRL_SBP | RCTRL_UPE | RCTRL_MPE | RCTRL_LPE | RCTRL_BAM | BSIZE_4096));
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
            Log::Warning("[i8254x] No 8254x device found!");
            return;
        }

        memBase = (device.header0.headerType ? device.header1.baseAddress0 : device.header0.baseAddress0) & 0xFFFFFFFFFFFFF000;

        memBaseVirt = (void*)Memory::GetIOMapping(memBase);

        Log::Info("[i8254x]    Base Address: ");
        Log::Write(device.header0.baseAddress0);
        Log::Write(", Base Address (virtual): ");
        Log::Write((uintptr_t)memBaseVirt);
        hasEEPROM = CheckForEEPROM();
        Log::Write(", EEPROM Present: ");
        Log::Write(hasEEPROM ? "true" : "false");

        if(!hasEEPROM){
            Log::Error("[i8254x] No EEPROM Present!");
            return;
        }

        int irqNum = device.header0.interruptLine;
        Log::Write(",IRQ: ");
        Log::Write(irqNum);

        APIC::IO::MapLegacyIRQ(irqNum);
        IDT::RegisterInterruptHandler(IRQ0 + irqNum, InterruptHandler);

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
        Log::Write(macAddr[5]);
        
        for(int i = 0; i < 128; i++ )
            WriteMem32(I8254_REGISTER_MTA + (i * 4), 0);

        WriteMem32(I8254_REGISTER_CTRL, ReadMem32(I8254_REGISTER_CTRL) | CTRL_SLU);

        WriteMem32(I8254_REGISTER_INT_MASK, 0x1F6DF); // Set the interrupt mask to enable all interrupts
    }
}