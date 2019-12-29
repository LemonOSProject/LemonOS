#pragma once

#include <stdint.h>

struct RSDPDescriptor {
 char signature[8];
 uint8_t checksum;
 char oemID[6];
 uint8_t revision;
 uint32_t rsdtAddress;
} __attribute__ ((packed));

struct RSDTHeader {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oemID[6];
  char oemTableID[8];
  uint32_t oemRevision;
  uint32_t creatorID;
  uint32_t creatorRevision;
};

namespace ACPI{
	extern RSDPDescriptor desc;
	extern RSDTHeader header;

	void Init();
  void Reset();
}