![banner](extra/lemonlt.png)

[![CI](https://github.com/LemonOSProject/LemonOS/actions/workflows/ci.yml/badge.svg)](https://github.com/LemonOSProject/LemonOS/actions/workflows/ci.yml)

Lemon OS is a UNIX-like 64-bit operating system written in C++.

## About Lemon OS (Work-in-progress branch)
This branch involves a new user-kernel API and a lot of kernel changes, hence a lot doesn't work right now. Big plans!

- Make things such as the UI and system architecture more unique and lemon-y, instead of writing another shitty nix clone
- Safer syscall API (much less unchecked memory accesses)
- vDSO to make system calls bc it's cool
- Repo reorganisation
- It turns out typing capital letters on filepaths is kinda annoying, so less of that
- Untie a lot of code from x86 to support ARM in the future :)
- Pay closer attention to how UNIX systems work in regards to filesystems and sockets
- Less shitter reimplementations of features that already exist on other *nixes 

Maybes:
- Reduce amount of external dependencies
- Get userspace working on other kernels
- Port a bunch of software known to work with mlibc (maintaining software is really 面倒くさい)

## [Website](https://lemonos.org)
## [Discord Server](https://discord.gg/NAYp6AUYWM)
## [Building Lemon OS](docs/Build/Building-Lemon-OS.md)

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
