# Lemon OS

The Lemon Operating System

Lemon OS is a 64-bit operating system written in C++.

(https://lemonos.org)

### Features
- Window Management
- ATA and SATA Driver
- Dynamic Linking
- Pseudoterminals and Terminal Emulator
- Intel 8254x Driver
- Freetype and Libpng Port
- [GnuBoy Port](https://github.com/fido2020/lemon-gnuboy)
- [DOOM Port](https://github.com/fido2020/LemonDOOM)

### Work In Progess
- FAT32 Filesystem support (read only)
- Network Stack

### System requirements
- 256 MB RAM (512 is more optimal)
- x86_64 Processor
- VESA VBE support

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
