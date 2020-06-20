#include <ata.h>
#include <atadrive.h>

#include <stdint.h>
#include <pci.h>
#include <logging.h>
#include <physicalallocator.h>
#include <memory.h>
#include <idt.h>
#include <devicemanager.h>
#include <apic.h>

namespace ATA{

	int port0 = 0x1f0;
	int port1 = 0x170;
	int controlPort0 = 0x3f6;
	int controlPort1 = 0x376;

	ATADiskDevice* drives[4];

	uint32_t busMasterPort;

	pci_device_t controllerDevice{
		nullptr, // Vendor Pointer
		0, // Device ID
		0, // Vendor ID
		"Generic ATA Compaitble Storage Controller", // Name
		0, // Bus
		0, // Slot
		0, // Func
		PCI_CLASS_STORAGE, // Mass Storage Controller
		0x01, // ATA Controller Subclass
		{},
		{},
		true, // Is a generic driver
	};

	inline void WriteRegister(uint8_t port, uint8_t reg, uint8_t value){
		outportb((port ? port1 : port0 ) + reg, value);
	}

	inline uint8_t ReadRegister(uint8_t port, uint8_t reg){
		return inportb((port ? port1 : port0 ) + reg);
	}
	
	inline void WriteControlRegister(uint8_t port, uint8_t reg, uint8_t value){
		outportb((port ? controlPort1 : controlPort0 ) + reg, value);
	}

	inline uint8_t ReadControlRegister(uint8_t port, uint8_t reg){
		return inportb((port ? controlPort1 : controlPort0 ) + reg);
	}


	int DetectDrive(int port, int drive){
		WriteRegister(port, ATA_REGISTER_DRIVE_HEAD, 0xa0 | (drive << 4)); // Drive

		for(int i = 0; i < 4; i++) inportb(0x3f6);

		WriteRegister(port, ATA_REGISTER_SECTOR_COUNT, 0);
		WriteRegister(port, ATA_REGISTER_LBA_LOW, 0);
		WriteRegister(port, ATA_REGISTER_LBA_MID, 0);
		WriteRegister(port, ATA_REGISTER_LBA_HIGH, 0);

		WriteRegister(port, ATA_REGISTER_COMMAND, 0xec); // Identify

		while(ReadRegister(port, ATA_REGISTER_STATUS) & 0x80);

		if(!ReadRegister(port, ATA_REGISTER_STATUS)){
			return 0;
		}

		int timer = 0xFFFFFF;
		while(timer--){
			if(!(ReadRegister(port, ATA_REGISTER_STATUS) & 0x80)) goto c1;
		}
		return 0;

	c1:
		if(ReadRegister(port, ATA_REGISTER_LBA_MID) || ReadRegister(port, ATA_REGISTER_LBA_HIGH)) return 0;

		return 1;
	}

	void IRQHandler(regs64_t* r){
		Log::Info("Drive IRQ!");
	}

	int Init(){
		controllerDevice.classCode = PCI_CLASS_STORAGE; // Storage Device
		controllerDevice.subclass = 0x1; // ATA Controller
		controllerDevice = PCI::RegisterPCIDevice(controllerDevice);

		if(controllerDevice.vendorID == 0xFFFF){
			return true; // No ATA Controller Found
		}

        Log::Info("Initializing ATA:");

        busMasterPort = controllerDevice.header0.baseAddress4 & 0xFFFFFFFC;

		Log::Write(" Bus Master: ");
		Log::Write(busMasterPort);

		IDT::RegisterInterruptHandler(IRQ0 + 14, IRQHandler);
		APIC::IO::MapLegacyIRQ(IRQ0 + 14);

		for(int i = 0; i < 2; i++){ // Port
			WriteControlRegister(i, 0, ReadControlRegister(i, 0) | 4); // Software Reset
			
			for(int z = 0; z < 4; z++) inportb(0x3f6);

			WriteControlRegister(i, 0, 0);

			for(int j = 0; j < 2; j++){ // Drive (master/slave)
				if(DetectDrive(i, j)){
					Log::Info("Found ATA Drive: Port: ");
					Log::Write(i);
					Log::Write(", ");
					Log::Write(j ? "slave" : "master");

					drives[i * 2 + j] = new ATADiskDevice(i, j);
					DeviceManager::devices->add_back(drives[i * 2 + j]);
				}
			}
		}

		return 0;
    }

	int Access(ATADiskDevice* drive, uint64_t lba, uint16_t count, void* buffer, bool write){
		if(count > 4){
			Log::Warning("ATA::Read was called with count > 4");
			return 1;
		}
		
		while(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80 || !(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x40));

		outportb(busMasterPort + ATA_BMR_CMD, 0);
		outportd(busMasterPort + ATA_BMR_PRDT_ADDRESS, drive->prdtPhys);

		outportb(busMasterPort + ATA_BMR_STATUS, inportb(busMasterPort +  ATA_BMR_STATUS) | 4 | 2); // Clear Error and Interrupt Bits

		if(write){
			outportb(busMasterPort + ATA_BMR_CMD, 0 /*Write */);
		} else {
			outportb(busMasterPort + ATA_BMR_CMD, 8 /* Read */);
		}
		
		while(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80);

		WriteRegister(drive->port, ATA_REGISTER_DRIVE_HEAD, 0x40 | (drive->drive << 4));
		WriteControlRegister(drive->port, 0, 0);

		for(int i = 0; i < 4; i++) inportb(0x3f6);

		WriteRegister(drive->port, ATA_REGISTER_SECTOR_COUNT, (count >> 8) & 0xFF);

		WriteRegister(drive->port, ATA_REGISTER_LBA_LOW, (lba >> 24) & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_MID, (lba >> 32) & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_HIGH, (lba >> 40) & 0xFF);

		WriteRegister(drive->port, ATA_REGISTER_SECTOR_COUNT, count & 0xFF);
		
		WriteRegister(drive->port, ATA_REGISTER_LBA_LOW, lba & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_MID, (lba >> 8) & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);

		while(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80 || !(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x40));

		if(write){
			WriteRegister(drive->port, ATA_REGISTER_COMMAND, 0x35); // 48-bit write DMA
		} else {
			WriteRegister(drive->port, ATA_REGISTER_COMMAND, 0x25); // 48-bit read DMA
		}

		for(int i = 0; i < 4; i++) inportb(0x3f6);

		if(write){
			outportb(busMasterPort + ATA_BMR_CMD, 0 /*Write*/ | 1 /*Start*/);
		} else {
			outportb(busMasterPort + ATA_BMR_CMD, 8 /*Read*/ | 1 /*Start*/);
		}
		
		while(!(inportb(busMasterPort + ATA_BMR_STATUS) & 0x4));// || (ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80));
		
		outportb(busMasterPort + ATA_BMR_CMD, 0);
		outportb(busMasterPort + ATA_BMR_STATUS, 4 | 2);

		if(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x1){
			Log::Warning("Disk Error!");
			return 1;
		}

		memcpy(buffer, drive->prdBuffer, count * 512);
		return 0;
	}
}