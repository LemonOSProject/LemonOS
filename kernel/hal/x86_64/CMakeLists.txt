target_sources(kernel.sys PRIVATE
    boot.cpp
    boot_alloc.cpp
    cpu.cpp
    idt.cpp
    page_map.cpp
    panic.cpp
    serial.cpp
    vmem.cpp
)
