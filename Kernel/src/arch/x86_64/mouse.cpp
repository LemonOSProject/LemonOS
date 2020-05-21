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

	bool dataUpdated = false;

	void Handler(regs64_t* regs) {
		switch (mouseCycle)
		{
		case 0:
			mouseData[0] = inportb(0x60);
			mouseCycle++;
			dataUpdated = true;
			break;
		case 1:
			mouseData[1] = inportb(0x60);
			mouseCycle++;
			dataUpdated = true;
			break;
		case 2:
			mouseData[2] = inportb(0x60);
			mouseCycle = 0;
			dataUpdated = true;
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

	bool DataUpdated() {
		bool updated = dataUpdated;
		if(dataUpdated)
			dataUpdated = false;
		return updated;
	}

	class MouseDevice : public FsNode{
	public:
		MouseDevice(char* name){
			strcpy(this->name, name);
		}
		uint32_t flags = FS_NODE_CHARDEVICE;

		size_t Read(size_t offset, size_t size, uint8_t *buffer){
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
	};

	MouseDevice mouseDev("mouse0");

	void Install()
	{
		uint8_t status;

		fs::RegisterDevice(&mouseDev);

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
		APIC::IO::MapLegacyIRQ(12);
	}

	int8_t* GetData() {
		return mouseData;
	}
}