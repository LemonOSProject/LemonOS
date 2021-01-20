![banner](Extra/lemonlt.png)

The Lemon Operating System

Lemon OS is a UNIX-like 64-bit operating system written in C++.
If you have any questions or concerns feel free to open a GitHub issue, join our Discord server or email me at computerfido@gmail.com.

Website: https://lemonos.org
Discord Server: https://discord.gg/NAYp6AUYWM

![Lemon OS Screenshot](Screenshots/image4.png)

### Prebuilt Image
[Latest Prebuilt Image](https://github.com/fido2020/Lemon-OS/releases/tag/0.2.2)

### Building
See [Building Lemon OS with Docker (Recommended)](https://github.com/fido2020/Lemon-OS/wiki/Building-Lemon-OS-with-Docker)
or [Building Lemon OS](https://github.com/fido2020/Lemon-OS/wiki/Building-Lemon-OS)

### Features
- Symmetric Multiprocessing (SMP)
- UNIX Domain Sockets
- Window Manager/Server (LemonWM)
- Graphical Shell
- Terminal Emulator
- Writable Ext2 Filesystem
- IDE and AHCI Driver
- Dynamic Linking
- Intel 8254x Ethernet Driver
- Ports including Freetype, Binutils and Python 3.8
- [mlibc](https://github.com/managarm/mlibc) C Library Port
- [GnuBoy Port](https://github.com/fido2020/lemon-gnuboy)
- [DOOM Port](https://github.com/fido2020/LemonDOOM)

### Work In Progress
- Network Stack
- NVMe Driver
- XHCI Driver

### System requirements
- 256 MB RAM (512 is closer to the optimal amount)
- x86_64 Processor
- 2 cores/CPUs recommended
- I/O APIC
- ATA or AHCI disk (AHCI *strongly* recommended)

## Repo Structure

| Directory     | Description                        |
| ------------- | ---------------------------------- |
| Applications/ | Userspace Applications             |
| Base/         | Files copied to disk               |
| Documentation/| Lemon OS Documentation             |
| Extra/        | (Currently) vector icons           |
| Kernel/       | Lemon Kernel                       |
| LibLemon/     | LibLemon (Lemon API)               |
| Toolchain/    | Cross Compiler Patches             |
| Ports/        | Build scripts and patches for ports|
| Resources/    | Images, fonts, etc.                |
| Scripts/      | Build Scripts                      |
| Screenshots/  | Screenshots                        |
| System/       | Core system programs and services  |

![Lemon OS Screenshot](Screenshots/image3.png)
![Lemon OS Screenshot](Screenshots/image.png)
![Lemon OS Screenshot](Screenshots/image2.png)
