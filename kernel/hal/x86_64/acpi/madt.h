#pragma once

#include <stdint.h>

namespace hal::acpi {

constexpr uint8_t MADT_LAPIC = 0;
constexpr uint8_t MADT_IOAPIC = 1;
constexpr uint8_t MADT_ISO = 2;
constexpr uint8_t MADT_NMI_SOURCE = 3;
constexpr uint8_t MADT_NMI = 4;
constexpr uint8_t MADT_LAPIC_ADDR_OVERRIDE = 5;
constexpr uint8_t MADT_X2APIC = 9;

constexpr uint8_t MADT_LAPIC_ENABLED = 1;
constexpr uint8_t MADT_LAPIC_ONLINE_CAPABLE = 2;

struct MADTEntry {
    uint8_t entry_type;
    uint8_t length;

    union {
        struct {
            uint8_t acpi_id;
            uint8_t apic_id;
            uint32_t flags;
        } __attribute__((packed)) lapic;

        struct {
            uint8_t id;
            uint8_t reserved;
            uint32_t address;
            uint32_t gsi_base;
        } __attribute__((packed)) ioapic;

        struct {
            uint8_t bus;
            uint8_t source;
            uint32_t gsi;
            uint16_t flags;
        } __attribute__((packed)) iso;

        struct {
            uint8_t source;
            uint8_t reserved;
            uint16_t flags;
            uint32_t gsi;
        } __attribute__((packed)) nmi_source;

        struct {
            uint8_t processor;
            uint16_t flags;
            uint8_t lint;
        } __attribute__((packed)) nmi;

        struct {
            uint16_t reserved;
            uint64_t address;
        } __attribute__((packed)) lapic_addr_override;

        struct {
            uint16_t reserved;
            uint32_t x2apic_id;
            uint32_t flags;
            uint32_t acpi_id;
        } x2apic;
    };
} __attribute__((packed));

}
