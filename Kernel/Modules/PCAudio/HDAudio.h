#pragma once

// Intel HD Audio Driver for Lemon OS

#include "Audio.h"

#include <Device.h>
#include <PCI.h>

namespace Audio {
	class IntelHDAudioController : public AudioController, PCIDevice {
	public:
		IntelHDAudioController(const PCIInfo& info);
	private:
		struct ControllerRegs {
			uint16_t globalCap; // Global Capabilities
			uint8_t minorVersion;
			uint8_t majorVersion;
			uint16_t outputPayloadCap; // Output Payload Capability
			uint16_t inputPayloadCap; // Input Payload Capability
			uint32_t globalControl; // Global Control
			uint16_t wakeEnable; // Wake Enable
			uint16_t wakeStatus; // Wake Status
			uint16_t globalStatus; // Global Status
			uint8_t reserved[6];
			uint16_t outputStreamPayloadCap; // Output Stream Payload Capability
			uint16_t inputStreamPayloadCap; // Input Stream Payload Capability
			uint8_t reserved2[4];
			uint32_t intControl; // Interrupt Control
			uint32_t intStatus; // Interrupt Status
			uint8_t reserved3[8];
			uint32_t wallClock; // Wall Clock Counter
			uint8_t reserved4[4];
			uint32_t streamSync; // Stream Synchronizatoin
			uint8_t reserved5[4];
			uint64_t corbBase; // CORB Base Address
			uint16_t corbWriteP; // CORB Write Pointer
			uint16_t corbReadP; // CORB Read Pointer
			uint8_t corbControl; // CORB Control
			uint8_t corbStatus; // CORB Status
			uint8_t corbSize; // CORB Size
			uint8_t reserved6;
			uint64_t rirbBase; // RIRB Base
			uint16_t rirbWriteP;
			uint16_t rIntCount; // Response Interrupt Count
			uint8_t rirbControl;
			uint8_t rirbStatus;
			uint8_t rirbSize;
			uint8_t reserved7;
			uint32_t immediateCmdOutputInterface; // Immediate Command Output Interface
			uint32_t immediateCmdInputInterface; // Immediate Command Output Interface
			uint16_t immediateCmdStatus;
			uint8_t reserved8[6];
			uint64_t dmaPositionBuffer;
			uint8_t reserved9[8];
			uint32_t streamDesc0Control : 24; // Input Stream Descriptor 0 Control
			uint8_t streamDesc0Status;
			uint32_t streamDesc0LinkPosInCurrentBuffer; // ISD0 Link Position in Current Buffer
			uint32_t streamDesc0CyclicBufferLength;
			uint16_t streamDesc0LastValidIndex;
			uint8_t reserved10[2];
			uint16_t streamDesc0FIFOSize;
			uint16_t streamDesc0Format;
			uint8_t reserved11[4];
			uint64_t streamDesc0BufferDescListPtr;
		} __attribute__((packed));

		ControllerRegs* cRegs;
	};
}