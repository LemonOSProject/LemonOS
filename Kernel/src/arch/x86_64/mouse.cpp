#include <mouse.h>

#include <idt.h>
#include <stddef.h>
#include <fs/filesystem.h>
#include <string.h>
#include <system.h>
#include <logging.h>
#include <apic.h>

namespace Mouse{
	int8_t mouseData[3];
	uint8_t mouseCycle;

	bool data_updated = false;

	void Handler(regs64_t* regs) {
		switch (mouseCycle)
		{
		case 0:
			mouseData[0] = inportb(0x60);
			mouseCycle++;
			data_updated = true;
			break;
		case 1:
			mouseData[1] = inportb(0x60);
			mouseCycle++;
			data_updated = true;
			break;
		case 2:
			mouseData[2] = inportb(0x60);
			mouseCycle = 0;
			data_updated = true;
			break;
		}
	}

	inline void Wait(uint8_t type)
	{
		int timeout = 100000;
		if (type == 0) {
			while (timeout--) //Data
				if ((inportb(0x64) & 1) == 1)
					return;
		} else
			while (timeout--) //Signal
				if ((inportb(0x64) & 2) == 0)
					return;
	}

	inline void Write(uint8_t data)
	{
		Wait(1);
		outportb(0x64, 0xD4);
		Wait(1);
		//Send data
		outportb(0x60, data);
	}

	uint8_t Read()
	{
		//Get's response from mouse
		Wait(0);
		return inportb(0x60);
	}

	fs_node_t mouseCharDev;
	size_t ReadDevice(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);

	void Install()
	{
		uint8_t status;  //unsigned char

		mouseCharDev.flags = FS_NODE_CHARDEVICE;
		mouseCharDev.read = ReadDevice;
		strcpy(mouseCharDev.name, "mouse0");
		fs::RegisterDevice(&mouseCharDev);

		Wait(1);
		outportb(0x64, 0xA8);

		//Enable the interrupts
		Wait(1);
		outportb(0x64, 0x20);
		Wait(0);
		status = (inportb(0x60) | 2);
		Wait(1);
		outportb(0x64, 0x60);
		Wait(1);
		outportb(0x60, status);

		Write(0xF6);
		Read();

		Write(0xF4);
		Read();

		IDT::RegisterInterruptHandler(IRQ0 + 12, Handler);
		APIC::IO::MapLegacyIRQ(IRQ0 + 12);
	}

	int8_t* GetData() {
		return mouseData;
	}

	bool DataUpdated() {
		bool updated = data_updated;
		if(data_updated)
			data_updated = false;
		return updated;
	}


	size_t ReadDevice(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		if(size < 3) return 0;

		if(DataUpdated())
			memcpy(buffer,mouseData, 3);
		else {
			buffer[0] = mouseData[0];
			buffer[1] = 0;
			buffer[2] = 0;
		}
		
		return 3;
	}
}