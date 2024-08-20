target_sources(kernel.sys PRIVATE
    acpi/acpi.cpp
    acpi/hpet.cpp
    
    boot.cpp
    boot_alloc.cpp
    cpu.cpp
    idt.cpp
    page_map.cpp
    panic.cpp
    serial.cpp
    vmem.cpp
)
