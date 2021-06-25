*~~It is recommended that you [build with docker](Building-Lemon-OS-with-Docker.md), building the toolchain will takes a long time.~~ Docker build Outdated*

## Prerequisites
_NOTE: Building on WSL2 is doable, however I strongly recommend an actual UNIX system._
* A UNIX-like build system
* An existing GCC toolchain
* [Meson](https://mesonbuild.com/Getting-meson.html)

## Linux
*NOTE: It is recommended that you install meson from pip, some distributions (e.g. Ubuntu) offer outdated meson packages*

### Arch Linux
```sh
sudo pacman -S base-devel autoconf python3 ninja wget python-pip nasm
```

### Debian, Ubuntu, etc.
```sh
sudo apt install build-essential autoconf libtool python3 python3-pip ninja-build nasm e2fsprogs dosfstools
```

## Cloning
Make sure you use `--recurse-submodules` to get the submodules
```sh
git clone https://github.com/fido2020/Lemon-OS.git --recurse-submodules
```

## Toolchain
The first step is to build the toolchain.
Open a terminal in the `Toolchain/` directory and run `./buildtoolchain.sh build`. This will build binutils, LLVM and limine for Lemon OS. This will take quite a long time (Can take from 20 minutes to and hour) so you may want to go and do something else.

You may run out of RAM whilst building LLVM (especially during the linking stage), so I recommend no more than **12 compile jobs (default) and no more than 4 link jobs (default) on 16GB of RAM** this can be changed by setting `JOBCOUNT` and `LINKCOUNT` environment variables

```sh
cd Toolchain
JOBCOUNT=12 LINKCOUNT=4 ./buildtoolchain.sh build
```

## Configuration
Go to `Scripts/` and run `./configure.sh` this will configure meson. It will then download and build zlib, libpng, freetype and nyancat.
```sh
cd Scripts
./configure.sh
```

## Build and run
Go to `Build/` and run `ninja install` \
```sh
cd Build
ninja install
```

To build the disk image run `ninja disk`\
```sh
ninja disk
```

### Running with QEMU/KVM
To run with QEMU run `ninja run`
```sh
ninja run
```

### Running with VirtualBox
To run with VirtualBox create a vm called "LemonOS". You'll need to symlink `Disks/Lemon.img` to `Disks/Lemon.hdd` Then run `ninja run-vbox`
```sh
ninja run-vbox
```

## Building Ports
To build the ports
```sh
cd Ports
./buildport.sh <port name> # e.g. ./buildport.sh nyancat
```