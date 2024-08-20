#include "acpi.h"

#include <le/vector.h>

#include <assert.h>

#include <cpu.h>

#include "hpet.h"
#include "logging.h"
#include "string.h"
#include "vmem.h"

#include "tables.h"

#include <le/lazy_constructed.h>

#define ACPI_RSDP_SIGNATURE "RSD PTR "

namespace hal::acpi {

void *rsdp_address = nullptr;
acpi_xsdp_t *rsdp_mapping;

Vector<void *> *acpi_tables;

static union {
    acpi_xsdt_t *xsdt;
    acpi_rsdt_t *rsdt;
};

void scan_table(const char sig[4], void (*table_fn)(void *), int scan_all) {
    for (auto *entry : *acpi_tables) {
        auto *header = (acpi_header_t *)entry;
        if (!strncmp(header->signature, sig, 4)) {
            table_fn(entry);

            if (!scan_all) {
                return;
            }
        }
    }
}

int scan_tables() {
    acpi_tables = new Vector<void *>;

    if (!rsdp_address) {
        log_error("acpi: RSDP never set");

        return 1;
    }

    rsdp_mapping = (acpi_xsdp_t*)rsdp_address;
    if (!rsdp_mapping) {
        log_error("acpi: Failed to map RSDP");

        return 1;
    }

    if (strncmp(rsdp_mapping->signature, ACPI_RSDP_SIGNATURE, 8)) {
        log_error("acpi: Invalid RSDP signature");

        return 1;
    }

    log_info("acpi: revision {}", rsdp_mapping->revision);

    auto entry_size = sizeof(uint32_t);
    if (rsdp_mapping->revision == 2) {
        // ACPI v2
        uintptr_t xsdt_address = rsdp_mapping->xsdt;
        assert(xsdt_address);

        xsdt = (acpi_xsdt_t*)create_io_mapping(xsdt_address, PAGE_SIZE_4K, mm::MemoryProtection::ro(), 0);
        assert(xsdt);

        entry_size = sizeof(uint64_t);
    } else if (rsdp_mapping->revision == 0) {
        // ACPI v1
        uintptr_t xsdt_address = rsdp_mapping->rsdt;
        assert(xsdt_address);

        rsdt = (acpi_rsdt_t*)create_io_mapping(xsdt_address, PAGE_SIZE_4K, mm::MemoryProtection::ro(), 0);
        assert(rsdt);
    } else {
        log_error("acpi: Unsupported ACPI revision {}", rsdp_mapping->revision);
        return 1;
    }

    auto num_entries = (rsdt->header.length - sizeof(acpi_header_t)) / entry_size;
    for (unsigned i = 0; i < num_entries; i++) {
        void *mapping = create_io_mapping(rsdt->tables[i], PAGE_SIZE_4K, mm::MemoryProtection::ro(), 0);

        acpi_tables->push_back(mapping);
    }

    scan_table("APIC", [](void *entry) {
        cpu::register_apic((acpi_madt_t *)entry);
    }, 0);

    scan_table("HPET", [](void *entry) {
        init_hpet((HPETTable *)entry);
    }, 0);

    for (auto *entry : *acpi_tables) {
        destroy_io_mapping(entry);
    }

    return 0;
}

}
