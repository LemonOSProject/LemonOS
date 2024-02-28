target_sources(kernel.sys PRIVATE
    boot.cpp
    boot_alloc.cpp
    cpu.cpp
    idt.cpp
    serial.cpp
    panic.cpp
)
