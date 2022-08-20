![banner](extra/lemonlt.png)

[![CI](https://github.com/LemonOSProject/LemonOS/actions/workflows/ci.yml/badge.svg)](https://github.com/LemonOSProject/LemonOS/actions/workflows/ci.yml)

Lemon OS is a UNIX-like 64-bit operating system written in C++.

## About Lemon OS
Lemon OS includes its own [modular kernel](kernel) with SMP and networking, [window server/compositor](apps/servers/lemonwm) and [userspace applications](apps) as well as [a collection of software ports](ports).

If you have any questions or concerns feel free to open a GitHub issue, join our [Discord server](https://discord.gg/NAYp6AUYWM) or email me at computerfido@gmail.com.

## [Website](https://lemonos.org)
## [Discord Server](https://discord.gg/NAYp6AUYWM)
## [Building Lemon OS](docs/Build/Building-Lemon-OS.md)

## Prebuilt Image
[Nightly Images](https://github.com/LemonOSProject/LemonOS/actions/workflows/ci.yml?query=is%3Asuccess+branch%3Amaster) - Go to latest job, `Lemon.img` located under Artifacts\
**You must be logged in to GitHub to download the image**

**Before running**
See [System Requirements](#system-requirements)

![Lemon OS Screenshot](extra/screenshots/image9.png)\
[More screenshots](extra/screenshots)
## Features
- Modular Kernel
- Symmetric Multiprocessing (SMP)
- UNIX/BSD Sockets
- Network Stack (UDP, TCP, DHCP)
- A small HTTP client/downloader called [steal](apps/utilities/steal)
- Window Manager/Server [LemonWM](apps/servers/lemonwm)
- [Terminal Emulator](apps/core/terminal)
- Writable Ext2 Filesystem
- IDE, AHCI and NVMe Driver
- Dynamic Linking
- [mlibc](https://github.com/managarm/mlibc) C Library Port
- [LLVM/Clang Port](https://github.com/LemonOSProject/llvm-project)
- [DOOM Port](https://github.com/LemonOSProject/LemonDOOM)
- [Audio Player (using ffmpeg)](Applications/AudioPlayer)

## Work In Progress
- XHCI Driver
- Intel HD Audio Driver

## Third Party

Lemon OS depends on:
[mlibc](https://github.com/managarm/mlibc), [Freetype](https://freetype.org/), [zlib](https://z-lib.org/), [libressl](https://www.libressl.org/), [ffmpeg](https://ffmpeg.org/), [libfmt](https://fmt.dev) and [libpng](http://www.libpng.org/pub/png/libpng.html).

[Optional ports](ports/) include LLVM/Clang, DOOM, Binutils and Python 3.8

[Various background images are located here](base/lemon/resources/backgrounds)

## System requirements
- 256 MB RAM (512 is more optimal)
- x86_64 Processor supporting [x86_64-v2 instructions](https://en.wikipedia.org/wiki/X86-64#Microarchitecture_levels) including SSE4.2
    - For QEMU/KVM use `-cpu host` or at least `-cpu Nehalem` see [this page](https://qemu-project.gitlab.io/qemu/system/target-i386.html)
- 2 or more CPU cores recommended
- I/O APIC
- ATA, NVMe or AHCI disk (AHCI *strongly* recommended)

For QEMU run with: \
```qemu-system-x86_64 Lemon.img --enable-kvm -cpu host -M q35 -smp 2 -m 1G -netdev user,id=net0 -device e1000,netdev=net0 -device ac97``` \
**KVM is strongly recommended**

## Repo Structure

| Directory          | Description                              |
| ------------------ | ---------------------------------------- |
| apps/              | Userspace Applications                   |
| base/              | Config, etc. Files copied to disk        |
| docs/              | Lemon OS Documentation                   |
| extra/             | (Currently) images and screenshots       |
| kernel/            | Lemon Kernel                             |
| lib/               | liblemon and liblemongui                 |
| ports/             | Build scripts and patches for ports      |
| scripts/           | Build Scripts                            |
| protocols/         | Interface definition files               |
