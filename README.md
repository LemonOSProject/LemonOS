# Lemon OS

The Lemon Operating System

Lemon OS is a 64-bit operating system written in C++.

(https://lemonos.org)

### Features
- Symmetric Multiprocessing (SMP)
- UNIX Domain Sockets
- Window Management
- ATA and SATA Driver
- Dynamic Linking
- Pseudoterminals and 256 colour compatible Terminal Emulator
- Intel 8254x Ethernet Driver
- Freetype and Libpng Port
- FAT32 Filesystem (readonly)
- [GnuBoy Port](https://github.com/fido2020/lemon-gnuboy)
- [DOOM Port](https://github.com/fido2020/LemonDOOM)

### Work In Progess
- Ext2 Support
- Network Stack

### System requirements
- 256 MB RAM (512 is more optimal)
- x86_64 Processor
- VESA VBE support
- I/O APIC (mostly relevant for Virtual Machines)

### Build Requirements
- UNIX-like system (e.g. Linux or Windows under WSL)
- GNU binutils, make and GCC
- git, meson (see crossfile in Scripts) and ninja
- Freetype
- Libpng
- zLib
- [mlibc](https://github.com/managarm/mlibc) C Library

## Repo Structure

| Directory     | Description                        |
| ------------- | ---------------------------------- |
| Applications/ | Userspace Applications             |
| Kernel/       | Lemon Kernel                       |
| LibLemon/     | LibLemon (Lemon API)               |
| Toolchain/    | Cross Compiler Patches             |
| Ports/        | Build scripts and patches for ports|
| Resources/    | Images, fonts, etc.                |
| Scripts/      | Build Scripts                      |
| Screenshots/  | Screenshots                        |

![Lemon OS Screenshot](Screenshots/image3.png)
![Lemon OS Screenshot](Screenshots/image2.png)
![Lemon OS Screenshot](Screenshots/image.png)
