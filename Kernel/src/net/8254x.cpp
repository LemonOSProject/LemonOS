#include <net/8254x.h>

#include <pci.h>
#include <logging.h>
#include <paging.h>
#include <physicalallocator.h>
#include <system.h>
#include <idt.h>
#include <timer.h>
#include <acpi.h>
#include <apic.h>
#include <net/net.h>
#include <vector.h>

namespace Network{
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

    int supportedDeviceCount = 23;

    void Intel8254x::InterruptHandler(Intel8254x* card, RegisterContext* r){
        card->OnInterrupt();
    }

    void Intel8254x::WriteMem32(uintptr_t address, uint32_t data){
        if(useIO){
            outportl(ioBase + address, data);
        } else {
            *((uint32_t*)(reinterpret_cast<uintptr_t>(memBaseVirt) + address)) = data;
        }
    }

    uint32_t Intel8254x::ReadMem32(uintptr_t address){
        if(useIO){
            return inportl(ioBase + address);
        } else {
            return *((uint32_t*)(reinterpret_cast<uintptr_t>(memBaseVirt) + address));
        }
    }

    uint16_t Intel8254x::ReadEEPROM(uint8_t addr) {
        uint32_t temp = 0;
        WriteMem32(I8254_REGISTER_EEPROM, 1 | (((uint32_t)addr) << 8));
        while (!((temp = ReadMem32(I8254_REGISTER_EEPROM)) & (1 << 4)));
        return (uint16_t)((temp >> 16) & 0xFFFF);
    }

    bool Intel8254x::CheckForEEPROM(){
        WriteMem32(I8254_REGISTER_EEPROM, 1);

        for (int i = 0; i < 1000; i++) {
            uint32_t reg = ReadMem32(I8254_REGISTER_EEPROM);
            if(reg & 0x10) return true;
        }

        return false;
    }

    void Intel8254x::DetectAndInitialize(){
        for(int i = 0; i < supportedDeviceCount; i++){
            if(PCI::FindDevice(supportedDevices[i], INTEL_VENDOR_ID)){
                Intel8254x* card = new Intel8254x(PCI::GetPCIDevice(supportedDevices[i], INTEL_VENDOR_ID));

                if(card->dState == DriverState::OK){
                    NetFS::GetInstance()->RegisterAdapter(card);
                }
                return;
            }
        }
        Log::Info("[i8254x] No i8254x Network Adapter found!");
    }

    void Intel8254x::OnInterrupt(){
        uint32_t status = ReadMem32(I8254_REGISTER_INT_READ);
        
        if(status & 0x4){
            Log::Info("[i8254x] Initializing Link...");

            WriteMem32(I8254_REGISTER_CTRL, ReadMem32(I8254_REGISTER_CTRL) | CTRL_SLU | CTRL_ASDE);

            UpdateLink();
        } else if(status & 0x80){
            do {
                rxTail = ReadMem32(I8254_REGISTER_RDESC_TAIL);
                if(rxTail == ReadMem32(I8254_REGISTER_RDESC_HEAD)) return;
                rxTail = (rxTail + 1) % RX_DESC_COUNT;

                if(!(rxDescriptors[rxTail].status & 0x1)) break;

                rxDescriptors[rxTail].status = 0;

                if(rxDescriptors[rxTail].length <= ETHERNET_MAX_PACKET_SIZE && cache.get_length()){
                    NetworkPacket* pkt = cache.remove_at(0);

                    pkt->length = rxDescriptors[rxTail].length;
                    memcpy(pkt->data, rxDescriptorsVirt[rxTail], pkt->length);
                    
                    queue.add_back(pkt);
                    packetSemaphore.Signal();
                } else {
                    // TODO: Do something that isn't dropping the packet when the cache is empty
                }

                WriteMem32(I8254_REGISTER_RDESC_TAIL, rxTail);
            } while(1);
        }
    }

    int Intel8254x::GetSpeed(){
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

    void Intel8254x::UpdateLink(){
        int _link =  (ReadMem32(I8254_REGISTER_STATUS) | STATUS_LINK_UP);
        Log::Info("[i8254x] Link %s, Speed: %d", (_link) ? "Up" : "Down", GetSpeed());
        
        if(_link){
            linkState = LinkUp;
            FindMainAdapter();
        } else {
            linkState = LinkDown;
        }  
    }

    void Intel8254x::InitializeRx(){
        uint64_t rxDescPhys = Memory::AllocatePhysicalMemoryBlock(); // The card wants a physical address
        rxDescriptors = (r_desc_t*)Memory::KernelAllocate4KPages(1);
        Memory::KernelMapVirtualMemory4K(rxDescPhys, (uintptr_t)rxDescriptors, 1);

        memset(rxDescriptors, 0, PAGE_SIZE_4K);

        uint32_t rxLow = rxDescPhys & 0xFFFFFFFF;
        uint32_t rxHigh = rxDescPhys >> 32;
        uint32_t rxLen = 4096; // Memory block size
        uint32_t rxHead = 0;
        uint32_t _rxTail = RX_DESC_COUNT - 1; // Offset from base

        rxDescriptorsVirt = (void**)kmalloc(RX_DESC_COUNT * sizeof(void*));

        WriteMem32(I8254_REGISTER_RDESC_LO, rxLow);
        WriteMem32(I8254_REGISTER_RDESC_HI, rxHigh);
        WriteMem32(I8254_REGISTER_RDESC_LEN, rxLen);
        WriteMem32(I8254_REGISTER_RDESC_HEAD, rxHead);
        WriteMem32(I8254_REGISTER_RDESC_TAIL, _rxTail);

        for(int i = 0; i < RX_DESC_COUNT; i++){
            r_desc_t* rxd = &rxDescriptors[i];
            uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
            rxd->addr = phys;
            rxd->status = 0;

            rxDescriptorsVirt[i] = Memory::KernelAllocate4KPages(1);
            Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)rxDescriptorsVirt[i], 1);
        }

        WriteMem32(I8254_REGISTER_RCTRL, (RCTRL_ENABLE | RCTRL_SBP | RCTRL_UPE | RCTRL_MPE | RCTRL_LPE | RCTRL_BAM | RCTRL_SECRC | BSIZE_4096));
    }
    
    void Intel8254x::InitializeTx(){
        uint64_t txDescPhys = Memory::AllocatePhysicalMemoryBlock(); // The card wants a physical address
        txDescriptors = (t_desc_t*)Memory::GetIOMapping(txDescPhys);
        uint32_t txLow = txDescPhys & 0xFFFFFFFF;
        uint32_t txHigh = txDescPhys >> 32;
        uint32_t txLen = 4096; // Memory block size
        uint32_t txHead = 0;
        uint32_t _txTail = RX_DESC_COUNT; // Offset from base

        txDescriptorsVirt = (void**)kmalloc(TX_DESC_COUNT * sizeof(void*));

        WriteMem32(I8254_REGISTER_TDESC_LO, txLow);
        WriteMem32(I8254_REGISTER_TDESC_HI, txHigh);
        WriteMem32(I8254_REGISTER_TDESC_LEN, txLen);
        WriteMem32(I8254_REGISTER_TDESC_HEAD, txHead);
        WriteMem32(I8254_REGISTER_TDESC_TAIL, _txTail);

        for(int i = 0; i < TX_DESC_COUNT; i++){
            t_desc_t* txd = &txDescriptors[i];
            uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
            txd->addr = phys;
            txd->status = 0;

            txDescriptorsVirt[i] = Memory::KernelAllocate4KPages(1);
            Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)txDescriptorsVirt[i], 1);
        }

        WriteMem32(I8254_REGISTER_TCTRL, (TCTRL_ENABLE | TCTRL_PSP));
    }

    Intel8254x::Intel8254x(PCIDevice& device) : NetworkAdapter(NetworkAdapterEthernet), pciDevice(device){
        assert(device.vendorID != 0xFFFF);

        txTail = rxTail = 0;

        memBase = device.GetBaseAddressRegister(0);

        if(device.BarIsIOPort(1)) {
            ioBase = device.GetBaseAddressRegister(1);
        } else if(device.BarIsIOPort(2)) {
            ioBase = device.GetBaseAddressRegister(2);
        } 

        memBaseVirt = (void*)Memory::GetIOMapping(memBase);
        hasEEPROM = CheckForEEPROM();

        Log::Info("[i8254x]    Base Address: %x, Base Address (virtual): %x, IO Base: %x, EEPROM Present: %s, Base Address 1: %x, Base Address 2: %x", device.GetBaseAddressRegister(0), memBaseVirt, ioBase, (hasEEPROM ? "true" : "false"), device.GetBaseAddressRegister(1), device.GetBaseAddressRegister(2));

        if(!hasEEPROM){
            Log::Error("[i8254x] No EEPROM Present!");
            dState = DriverState::Error;
            return;
        }

        int irqNum = device.AllocateVector(PCIVectors::PCIVectorAny);
        Log::Write(",IRQ: ");
        Log::Write(irqNum);
        IDT::RegisterInterruptHandler(irqNum, reinterpret_cast<isr_t>(&Intel8254x::InterruptHandler), this);

        uint8_t macAddr[6];

        pciDevice.EnableBusMastering();

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

        mac = macAddr;

        Log::Info("[i8254x] MAC Address: %x:%x:%x:%x:%x:%x",
            mac[0], mac[1],mac[2], mac[3], mac[4], mac[5]);

        char tempName[NAME_MAX];
        char bus[16];
        char slot[16];

        itoa(pciDevice.bus, bus, 10);
        itoa(pciDevice.slot, slot, 10);

        strcpy(tempName, "e1k");
        strcat(tempName, bus);
        strcat(tempName, "s");
        strcat(tempName, slot);

        SetInstanceName(tempName); // Name format: e1k%pciBus%s%pciSlot%
        SetDeviceName("Intel 8254x Ethernet Adapter");

        WriteMem32(I8254_REGISTER_CTRL, ReadMem32(I8254_REGISTER_CTRL) | CTRL_SLU | CTRL_ASDE);

        InitializeRx();
        InitializeTx();

        dState = DriverState::OK;

        maxCache = 256;
        for(unsigned i = 0; i < maxCache; i++){
            cache.add_back(new NetworkPacket()); // Fill the cache
        }

        WriteMem32(I8254_REGISTER_INT_MASK, 0x1F6DF); // Set the interrupt mask to enable all interrupts
        UpdateLink();
    }
    
    void Intel8254x::SendPacket(void* data, size_t len){
        t_desc_t* txd = &(txDescriptors[txTail]);

        memcpy(txDescriptorsVirt[txTail], data, len);
        txd->length = len;
        txd->cmd = TCMD_EOP | TCMD_IFCS | TCMD_RS;
        
        txTail = (txTail + 1) % TX_DESC_COUNT;

        WriteMem32(I8254_REGISTER_TDESC_TAIL, txTail);
    }
}