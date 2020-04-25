# Lemon OS

The Lemon Operating System

### Features
- Window Management
- Freetype Port
- ATA Driver
- GnuBoy Port (not incl)
- Dynamic Linking
- Pseudoterminals and Terminal Emulator
- [DOOM Port](https://github.com/fido2020/LemonDOOM)

### Work In Progess
- FAT32 Filesystem support
- Intel 8254x Driver
- AHCI Driver

### System requirements
- 128 MB RAM
- x86_64 Processor
- VESA VBE support

### Build Requirements
- UNIX-like system (e.g. Linux or Windows under WSL)
- GNU binutils, make and GCC (yet to provide patch files)
- meson (see crossfile in Scripts) and ninja
- Freetype
- [mlibc](https://github.com/managarm/mlibc) C Library

## Repo Structure

| Directory     | Description            |
| ------------- | ---------------------- |
| Applications/ | Userspace Applications |
| InitrdWriter/ | Ramdisk creation tool  |
| Kernel/       | Lemon Kernel           |
| LibLemon/     | LibLemon (Lemon API)   |
| Patches/      | Patch files            |
| Scripts/      | Build Scripts          |
| Screenshots/  | Screenshots            |

![Lemon OS Screenshot](Screenshots/image3.png)
![Lemon OS Screenshot](Screenshots/image2.png)
![Lemon OS Screenshot](Screenshots/image.png)
