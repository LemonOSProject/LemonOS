# Lightweight AML Interpreter

LAI is an interpreter for AML, the ACPI Machine Language. AML is an integral component of modern BIOS and UEFI firmware, both on x86(_64) machines and ARM servers.
As an AML interpreter, LAI is used by OS kernels to implement support for ACPI.

LAI has been split up into 2 parts:
  - Core, The main parser/interpreter
  - Helpers, Some extra functions that help interfacing with the ACPI API

## Documentation

- [Core API Documentation](https://github.com/qword-os/lai/wiki/Core-API-Documentation)
- [Helper API Documentation](https://github.com/qword-os/lai/wiki/Helper-API-Documentation)
