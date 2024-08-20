#pragma once

namespace hal::acpi {

extern void *rsdp_address;

[[ nodiscard ]] int scan_tables();

}
