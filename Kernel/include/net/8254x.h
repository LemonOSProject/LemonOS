#pragma once

#include <net/networkadapter.h>
#include <pci.h>

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

namespace Network{
    class Intel8254x : public NetworkAdapter {
        typedef struct {
            uint64_t addr; // Buffer Address
            uint16_t length; // Length
            uint16_t checksum; // Packet Checksum
            uint8_t status; // Status
            uint8_t errors; // Errors
            uint16_t special; // Special
        } __attribute__((packed)) r_desc_t;

        typedef struct {
            uint64_t addr; // Buffer Address
            uint16_t length;
            uint8_t cso; // Checksum Offset
            uint8_t cmd; // Command
            uint8_t reserved : 4; // Reserved
            uint8_t status : 4; // Status
            uint8_t css; // Checksum start field
            uint16_t special; // Special
        } __attribute__((packed)) t_desc_t;

        r_desc_t* rxDescriptors;
        t_desc_t* txDescriptors;
        void** txDescriptorsVirt;
        void** rxDescriptorsVirt;

        unsigned txTail = 0;
        unsigned rxTail = 0;

        uint64_t memBase;
        void* memBaseVirt;
        uint64_t ioBase;

        bool useIO = false;
        bool hasEEPROM = false;

        const char* deviceName = "Intel 8254x Compaitble Ethernet Controller";

        PCIDevice& pciDevice;

        void WriteMem32(uintptr_t address, uint32_t data);
        uint32_t ReadMem32(uintptr_t address);
        uint16_t ReadEEPROM(uint8_t addr);
        bool CheckForEEPROM();

        int GetSpeed();

        void InitializeRx();
        void InitializeTx();

        void UpdateLink();

        public:
        Intel8254x(PCIDevice& device);

        void Interrupt();
        static void DetectAndInitialize();

        void SendPacket(void* data, size_t len);
    };
}