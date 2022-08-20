project(lemon-kernel)

cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LEMON_ARCH "x86_64")

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

if(NOT CMAKE_SYSTEM_NAME MATCHES Lemon)
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} should be 'Lemon'")
endif()

include_directories(include)
include_directories($ENV{LEMON_BUILDROOT}/thirdparty/frigg/include)
include_directories($ENV{LEMON_BUILDROOT}/thirdparty/cxxshim/stage2/include)

if("${LEMON_ARCH}" STREQUAL "x86_64")
    include_directories(include/Arch/x86_64)

    enable_language(ASM_NASM)
    if(NOT CMAKE_ASM_NASM_COMPILER_LOADED)
        message(FATAL_ERROR "Failed to find NASM assembler!")
    endif()
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -i${CMAKE_CURRENT_SOURCE_DIR}/src/Arch/x86_64 \
        -felf64 -w+gnu-elf-extensions -o <OBJECT> <SOURCE>")
else()
    message(FATAL_ERROR "Unsupported architecture ${LEMON_ARCH}")
endif()

add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)
add_compile_options(-Wno-write-strings -Wno-unused-parameter -Wno-sign-compare -Wno-deprecated-volatile -Wno-non-c-typedef-for-linkage)
add_compile_options(-ffreestanding -nostdlib)
add_compile_options(-mno-mmx -mno-sse -mno-sse2)
add_compile_options(-mcmodel=large -mno-red-zone -fno-pic)

# Inline functions even with O0
add_compile_options(-O0)
add_compile_options(-fno-builtin -finline-functions)
add_compile_options(-g)

set(KERNEL_SRC
    src/Assert.cpp
    src/CharacterBuffer.cpp
    src/Device.cpp
    src/Debug.cpp
    src/Hash.cpp
    src/Lemon.cpp
    src/Lock.cpp
    src/Logging.cpp
    src/Math.cpp
    src/Panic.cpp
    src/Runtime.cpp
    src/SharedMemory.cpp
    src/Streams.cpp
    src/String.cpp
    src/UserIO.cpp

    src/Audio/Audio.cpp

    src/Video/Video.cpp
    src/Video/VideoConsole.cpp

    src/Fs/Fat32.cpp
    src/Fs/Filesystem.cpp
    src/Fs/FsNode.cpp
    src/Fs/FsVolume.cpp
    src/Fs/Pipe.cpp
    src/Fs/TAR.cpp
    src/Fs/Tmp.cpp
    src/Fs/VolumeManager.cpp

    src/MM/AddressSpace.cpp
    src/MM/KMalloc.cpp
    src/MM/VMObject.cpp

    src/Net/NetworkAdapter.cpp
    src/Net/Socket.cpp
    src/Net/Net.cpp
    src/Net/Interface.cpp
    src/Net/IPSocket.cpp
    src/Net/UDP.cpp
    src/Net/TCP.cpp

    src/Objects/Interface.cpp
    src/Objects/KObject.cpp
    src/Objects/Message.cpp
    src/Objects/Process.cpp
    src/Objects/Service.cpp

    src/Storage/AHCIController.cpp
    src/Storage/AHCIPort.cpp
    src/Storage/ATA.cpp
    src/Storage/ATADrive.cpp
    src/Storage/DiskDevice.cpp
    src/Storage/GPT.cpp
    src/Storage/NVMe.cpp
    src/Storage/NVMeNamespace.cpp
    src/Storage/PartitionDevice.cpp

    src/TTY/PTMX.cpp
    src/TTY/PTY.cpp

    src/USB/XHCI.cpp

    src/sys/le_core.cpp
    src/sys/le_mm.cpp
    src/sys/sys_vfs.cpp
    src/sys/syscall.cpp
)

set(KERNEL_SRC_x86_64
    src/Arch/x86_64/ACPI.cpp
    src/Arch/x86_64/APIC.cpp
    src/Arch/x86_64/CPUID.cpp
    src/Arch/x86_64/ELF.cpp
    src/Arch/x86_64/HAL.cpp
    src/Arch/x86_64/IDT.cpp
    src/Arch/x86_64/PS2.cpp
    src/Arch/x86_64/Modules.cpp
    src/Arch/x86_64/Paging.cpp
    src/Arch/x86_64/PCI.cpp
    src/Arch/x86_64/PhysicalAllocator.cpp
    src/Arch/x86_64/Scheduler.cpp
    src/Arch/x86_64/Serial.cpp
    src/Arch/x86_64/SMP.cpp
    src/Arch/x86_64/SSP.cpp
    src/Arch/x86_64/StringSafe.cpp
    src/Arch/x86_64/Symbols.cpp
    src/Arch/x86_64/Thread.cpp
    src/Arch/x86_64/Timer.cpp
    src/Arch/x86_64/TSS.cpp

    src/Arch/x86_64/Entry.asm
    src/Arch/x86_64/IDT.asm
    src/Arch/x86_64/Memcpy.asm
    src/Arch/x86_64/SignalTrampoline.asm
    src/Arch/x86_64/SMPTrampoline.asm
    src/Arch/x86_64/Syscall.asm
    src/Arch/x86_64/TSS.asm
)

add_link_options(-z max-page-size=0x1000 -ffreestanding -nostdlib)

add_library(hiraku SHARED hiraku/syscalls.cpp)
target_compile_options(hiraku PRIVATE -fPIC -nostdlib)
target_include_directories(hiraku PRIVATE $ENV{LEMON_BUILDROOT}/lib/lemon/include)

add_library(kernel_obj OBJECT ${KERNEL_SRC} ${KERNEL_SRC_x86_64})

add_custom_command(
    OUTPUT hiraku-embed.o
    DEPENDS hiraku
    COMMAND objcopy -I binary -O elf64-x86-64 $<TARGET_FILE:hiraku> --rename-section .data=.hiraku hiraku-embed.o 
    VERBATIM
)

add_custom_command(
    OUTPUT ksyms.asm
    DEPENDS kernel_obj
    COMMAND $ENV{LEMON_BUILDROOT}/scripts/genkernelsyms.py ksyms.asm $<TARGET_OBJECTS:kernel_obj>
    VERBATIM
)

add_executable(kernel.sys
    src/Kernel.cpp ksyms.asm hiraku-embed.o $<TARGET_OBJECTS:kernel_obj>
)
target_link_options(kernel.sys PRIVATE 
    -T ${CMAKE_CURRENT_SOURCE_DIR}/linkscript-x86_64.ld
)

add_subdirectory(modules)

install(TARGETS kernel.sys
    RUNTIME DESTINATION lemon)
