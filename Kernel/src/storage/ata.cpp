#pragma once

#include <ata.h>
#include <atadrive.h>

#include <stdint.h>
#include <pci.h>
#include <logging.h>
#include <physicalallocator.h>
#include <memory.h>
#include <idt.h>
#include <devicemanager.h>

namespace ATA{

	int port0 = 0x1f0;
	int port1 = 0x170;
	int master = 0xa0;
	int slave = 0xb0;

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

    inline void SendCommand(uint8_t drive, uint8_t command){
        outportb((drive % 2) ? slave : master,command);
    }

	inline void WriteRegister(uint8_t port, uint8_t reg, uint8_t value){
		outportb((port ? port1 : port0 ) + reg, value);
	}

	inline uint8_t ReadRegister(uint8_t port, uint8_t reg){
		return inportb((port ? port1 : port0 ) + reg);
	}

	int DetectDrive(int port, int drive){
		WriteRegister(port, ATA_REGISTER_DRIVE_HEAD, (drive ? slave : master)); // Drive
		WriteRegister(port, ATA_REGISTER_COMMAND, 0xec); // Identify

		WriteRegister(port, ATA_REGISTER_SECTOR_COUNT, 0);
		WriteRegister(port, ATA_REGISTER_LBA_LOW, 0);
		WriteRegister(port, ATA_REGISTER_LBA_MID, 0);
		WriteRegister(port, ATA_REGISTER_LBA_HIGH, 0);

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
		return;
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

		for(int i = 0; i < 2; i++){ // Port
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
    }

	int Read(ATADiskDevice* drive, uint64_t lba, uint16_t count, void* buffer){
		if(count > 4){
			Log::Warning("ATA::Read was called with count > 4");
			return 1;
		}

		outportb(busMasterPort + (drive->port ? ATA_BMR_CMD_SECONDARY : ATA_BMR_CMD), 0);
		outportd(busMasterPort + (drive->port ? ATA_BMR_PRDT_ADDRESS_SECONDARY : ATA_BMR_PRDT_ADDRESS), drive->prdtPhys);

		outportb(busMasterPort + (drive->port ? ATA_BMR_STATUS_SECONDARY : ATA_BMR_STATUS), inportb(busMasterPort + (drive->port ? ATA_BMR_STATUS_SECONDARY : ATA_BMR_STATUS)) | 4 | 2); // Clear Error and Interrupt Bits

		int driveReg = drive->drive ? 0x50 /*Slave*/ : 0x40 /*Master*/;
		WriteRegister(drive->port, ATA_REGISTER_DRIVE_HEAD, driveReg);

		for(int i = 0; i < 4; i++) inportb(0x3f6);

		while(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80);
		
		WriteRegister(drive->port, ATA_REGISTER_SECTOR_COUNT, (count >> 8) & 0xFF);

		WriteRegister(drive->port, ATA_REGISTER_LBA_LOW, (lba >> 24) & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_MID, (lba >> 32) & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_HIGH, (lba >> 40) & 0xFF);

		WriteRegister(drive->port, ATA_REGISTER_SECTOR_COUNT, count & 0xFF);
		
		WriteRegister(drive->port, ATA_REGISTER_LBA_LOW, lba & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_MID, (lba >> 8) & 0xFF);
		WriteRegister(drive->port, ATA_REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);
		
		for(int i = 0; i < 4; i++) inportb(0x3f6);
		
		while(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x80);

		WriteRegister(drive->port, ATA_REGISTER_COMMAND, 0x25); // 48-bit read DMA

		for(int i = 0; i < 4; i++) inportb(0x3f6);

		outportb(busMasterPort + (drive->port ? ATA_BMR_CMD_SECONDARY : ATA_BMR_CMD), 8 /* Read */ | 1 /* Start*/);

		while(!(inportb(busMasterPort + (drive->port ? ATA_BMR_STATUS_SECONDARY : ATA_BMR_STATUS)) & 0x4) && !(inportb(busMasterPort + (drive->port ? ATA_BMR_STATUS_SECONDARY : ATA_BMR_STATUS)) & 0x2)) inportb(0x3f6);//Log::Write("i");

		outportb(busMasterPort + (drive->port ? ATA_BMR_CMD_SECONDARY : ATA_BMR_CMD), 0);

		if(ReadRegister(drive->port, ATA_REGISTER_STATUS) & 0x2){
			Log::Warning("Disk Error!");
			return 1;
		}

		memcpy(buffer, drive->prdBuffer, count * 512);
		return 0;
	}
}