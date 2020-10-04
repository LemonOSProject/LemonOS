#pragma once

#include <stdint.h>
#include <acpispec/tables.h>
#include <lai/helpers/pci.h>
#include <list.h>

typedef struct MADT{
  acpi_header_t header;
  uint32_t localAPICAddress;
  uint32_t flags;
} __attribute__ ((packed)) acpi_madt_t;

typedef struct MADTEntry{
  uint8_t type;
  uint8_t length;
} __attribute__ ((packed)) acpi_madt_entry_t;

typedef struct LocalAPIC{ // Local APIC - Type 0
  acpi_madt_entry_t entry; // MADT Entry Structure
  uint8_t processorID; // ACPI Processor ID
  uint8_t apicID; // APIC ID
  uint32_t flags; // Flags - (bit 0 = enabled, bit 1 = online capable)
} __attribute__ ((packed)) apic_local_t;

typedef struct IOAPIC{ // I/O APIC - Type 1
  acpi_madt_entry_t entry; // MADT Entry Structure
  uint8_t apicID; // APIC ID
  uint8_t reserved; // Reserved
  uint32_t address; // Address of I/O APIC
  uint32_t gSIB; // Global System Interrupt Base
} __attribute__ ((packed)) apic_io_t;

typedef struct ISO{ // Interrupt Source Override - Type 2
  acpi_madt_entry_t entry; // MADT Entry Structure
  uint8_t busSource; // Bus Source
  uint8_t irqSource; // IRQ Source
  uint32_t gSI; // Global System Interrupt
  uint16_t flags; // Flags
} __attribute__ ((packed)) apic_iso_t;

typedef struct NMI{ // Non-maskable Interrupt - Type 4
  acpi_madt_entry_t entry; // MADT Entry Structure
  uint8_t processorID; // ACPI Processor ID (255 for all processors)
  uint16_t flags; // Flags
  uint8_t lINT; // LINT Number (0 or 1)
} __attribute__ ((packed)) apic_nmi_t;

typedef struct LocalAPICAddressOverride { // Local APIC Address Override - Type 5
  acpi_madt_entry_t entry; // MADT Entry Structure
  uint16_t reserved; // Reserved
  uint64_t address; // 64-bit Address of Local APIC
} __attribute__ ((packed)) apic_local_address_override_t;

typedef struct PCIMCFGBaseAddress{
  uint64_t baseAddress; // Base address of configuration space
  uint16_t segmentGroupNumber; // PCI Segment group number
  uint8_t startPCIBusNumber; // Start PCI bus number decoded by this host bridge
  uint8_t endPCIBusNumber; // End PCI bus number decoded by this host bridge 
  uint32_t reserved;
} __attribute__ ((packed)) pci_mcfg_base_adress_t;

typedef struct PCIMCFG {
  acpi_header_t header;
  uint64_t reserved;
  PCIMCFGBaseAddress baseAddresses[];
} __attribute__ ((packed)) pci_mcfg_table_t;

namespace ACPI{
  extern uint8_t processors[];
  extern int processorCount;
  
	extern List<apic_iso_t*>* isos;

	extern acpi_xsdp_t* desc;
	extern acpi_rsdt_t* rsdtHeader;
  extern pci_mcfg_table_t* mcfg;

	void Init();
  void SetRSDP(acpi_xsdp_t* p);
  void SetRSDP(acpi_xsdp_t* p);

  void Reset();
}