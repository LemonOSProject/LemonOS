*It is recommended that you [build with docker](Building-Lemon-OS-with-Docker.md), building the toolchain will takes a long time.*

## Prerequisites
_NOTE: Building on WSL2 is doable, however I strongly recommend an actual UNIX system._
* A UNIX-like build system
* An existing GCC toolchain
* [Meson](https://mesonbuild.com/Getting-meson.html)

As well as the following (Arch) packages:\
`base-devel`, `autoconf`, `python3`, `ninja`, `wget`, `qemu`, `pip`, `nasm`\
Or Debian, etc.:\
`build-essential`, `autoconf`, `python3`, `python3-pip`, `ninja-build`, `qemu-utils`, `nasm`

## Cloning
Make sure you use `--recurse-submodules` to get the submodules
`git clone https://github.com/fido2020/Lemon-OS.git --recurse-submodules`

## Toolchain
The first step is to build the toolchain.
Open a terminal in the `Toolchain/` directory and run `./buildtoolchain.sh build`. This will build binutils and LLVM for Lemon OS. This will take quite a long time (Can take from 20 minutes to and hour) so you may want to go and do something else. You may run out of RAM whilst building LLVM, so I recommend no more that 5-6 jobs with 8 GB and 10-11 jobs with 16 GB of RAM, this can be changed by setting ```JOBCOUNT``` environment variable/
## Configuration
Go to `Scripts/` and run `./configure.sh` this will configure meson for the Kernel, LibLemon and the Applications. It will then download and build zlib, libpng, freetype and nyancat.

## Creating/Obtaining disk image
Install the GRUB package for your distro and run `Scripts/createdisk.sh`. This will create a disk image with limine as BIOS bootloader and GRUB for EFI

## Finally build and run
In the main directory run `make all` to build everything. Finally `make run` to run Lemon OS with QEMU/KVM, or run Disks/Lemon.img in your favourite VM.
