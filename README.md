![banner](Extra/lemonlt.png)

Lemon OS is a UNIX-like 64-bit operating system written in C++.

## About Lemon OS
Lemon OS includes its own [kernel](Kernel) with SMP and networking, [window server/compositor](System/LemonWM) and [userspace applications](Applications) as well as [a collection of software ports](Ports).

If you have any questions or concerns feel free to open a GitHub issue, join our [Discord server](https://discord.gg/NAYp6AUYWM) or email me at computerfido@gmail.com.

[Website](https://lemonos.org) \
[Discord Server](https://discord.gg/NAYp6AUYWM)

![Lemon OS Screenshot](Screenshots/image5.png)\
[More screenshots](Screenshots)

## Building
- [Building Lemon OS with Docker (Recommended)](Documentation/Build/Building-Lemon-OS-with-Docker.md)
- [Building Lemon OS](Documentation/Build/Building-Lemon-OS.md)

## Prebuilt Image
[Latest Prebuilt Image](https://github.com/LemonOSProject/LemonOS/releases/latest)

## Features
- Symmetric Multiprocessing (SMP)
- UNIX/BSD Sockets (local/UNIX domain and internet)
- Network Stack (UDP, TCP, DHCP)
- A small HTTP client/downloader called [steal](Applications/steal)
- Window Manager/Server [LemonWM](Applications/LemonWM)
- [Terminal Emulator](Applications/Terminal)
- Writable Ext2 Filesystem
- IDE, AHCI and NVMe Driver
- Dynamic Linking
- Intel 8254x Ethernet Driver
- Ports including Freetype, Binutils and Python 3.8
- [mlibc](https://github.com/managarm/mlibc) C Library Port
- [GnuBoy Port](https://github.com/LemonOSProject/lemon-gnuboy)
- [DOOM Port](https://github.com/LemonOSProject/LemonDOOM)

## Work In Progress
- XHCI Driver
- Clang/LLVM Port

## System requirements
- 256 MB RAM (512 is more optimal)
- x86_64 Processor
- 2 cores/CPUs recommended
- I/O APIC
- ATA, NVMe or AHCI disk (AHCI *strongly* recommended)

## Repo Structure

| Directory     | Description                        |
| ------------- | ---------------------------------- |
| Applications/ | Userspace Applications             |
| Base/         | Config, etc. Files copied to disk  |
| Documentation/| Lemon OS Documentation             |
| Extra/        | (Currently) vector icons           |
| Kernel/       | Lemon Kernel                       |
| LibLemon/     | LibLemon (Lemon API)               |
| Toolchain/    | Toolchain build scripts and patches|
| Ports/        | Build scripts and patches for ports|
| Resources/    | Images, fonts, etc.                |
| Scripts/      | Build Scripts                      |
| Screenshots/  | Screenshots                        |
| System/       | Core system programs and services  |