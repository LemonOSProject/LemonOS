# Lemon OS

The Lemon Operating System

Lemon OS is a UNIX-like 64-bit operating system written in C++.

(https://lemonos.org)

![Lemon OS Screenshot](Screenshots/image.png)

### Building
See [Building Lemon OS](https://github.com/fido2020/Lemon-OS/wiki/Building-Lemon-OS)

### Features
- Symmetric Multiprocessing (SMP)
- UNIX Domain Sockets
- Window Manager/Server (LemonWM)
- Graphical Shell
- Writable Ext2 Filesystem
- IDE and AHCI Driver
- Dynamic Linking
- Terminal Emulator w/ 256 colour support
- Intel 8254x Ethernet Driver
- Freetype and Libpng Port
- [mlibc](https://github.com/managarm/mlibc) C Library Port
- [GnuBoy Port](https://github.com/fido2020/lemon-gnuboy)
- [DOOM Port](https://github.com/fido2020/LemonDOOM)

### Work In Progess
- Network Stack

### System requirements
- 256 MB RAM (512 is more optimal)
- x86_64 Processor
- VESA VBE support
- 2 cores/CPUs recommended
- I/O APIC (mostly relevant for Virtual Machines)

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