# Lemon OS

The Lemon Operating System

### Features
- Window Management
- Freetype and Libpng Port
- ATA and SATA Driver
- [GnuBoy Port](https://github.com/fido2020/lemon-gnuboy)
- Dynamic Linking
- Pseudoterminals and Terminal Emulator
- [DOOM Port](https://github.com/fido2020/LemonDOOM)

### Work In Progess
- FAT32 Filesystem support (read only)
- Intel 8254x Driver

### System requirements
- 256 MB RAM
- x86_64 Processor
- VESA VBE support

### Build Requirements
- UNIX-like system (e.g. Linux or Windows under WSL)
- GNU binutils, make and GCC (yet to provide patch files)
- meson (see crossfile in Scripts) and ninja
- Freetype 2.10.1 (patch in Patches)
- Libpng 1.6.37 (patch in Patches)
- zLib (no patch needed)
- [mlibc](https://github.com/managarm/mlibc) C Library

## Repo Structure

| Directory     | Description            |
| ------------- | ---------------------- |
| Applications/ | Userspace Applications |
| InitrdWriter/ | Ramdisk creation tool  |
| Kernel/       | Lemon Kernel           |
| LibLemon/     | LibLemon (Lemon API)   |
| Patches/      | Patch files            |
| Resources/    | Images, fonts, etc.    |
| Scripts/      | Build Scripts          |
| Screenshots/  | Screenshots            |

![Lemon OS Screenshot](Screenshots/image3.png)
![Lemon OS Screenshot](Screenshots/image2.png)
![Lemon OS Screenshot](Screenshots/image.png)
