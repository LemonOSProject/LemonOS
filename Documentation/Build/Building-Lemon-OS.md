### *Building the toolchain can take an extremely long time, a Docker image has been built. Consider [Building Lemon OS with Docker](Building-Lemon-OS-with-Docker.md)*



## Prerequisites
_NOTE: Building on WSL2 is doable, however I strongly recommend an actual UNIX system._
* A UNIX-like build system
* An existing GCC toolchain
* Root access
* [Meson](https://mesonbuild.com/Getting-meson.html)

## Linux
*NOTE: It is recommended that you install meson from pip, some distributions (e.g. Ubuntu) offer outdated meson packages*

### Arch Linux
```sh
sudo pacman -S base-devel git cmake autoconf python3 ninja wget python-pip nasm help2man
```

### Debian, Ubuntu, etc.
```sh
sudo apt install build-essential git cmake curl autoconf libtool python3 python3-pip ninja-build nasm e2fsprogs dosfstools help2man
```

## Install pip packages
Install `xbstrap` through pip. `meson` can also be installed through pip.
```sh
python3 -m pip install xbstrap meson
```

## Initialize xbstrap
Create a new directory called Build and run `xbstrap init ..` inside it.
```sh
mkdir Build
cd Build
xbstrap init ..
```

## Toolchain
The next step is to build the toolchain.

```sh
xbstrap install-tool --all
```

This will build binutils, LLVM, automake, libtool and limine for Lemon OS. This will take quite a long time (Can take from 20 minutes to an hour) so you may want to go and do something else.

You may run out of RAM whilst building LLVM (especially during the linking stage), so I recommend no more than **12 compile jobs (default determined by ninja, based on CPU thread count) on 16GB of RAM** I have changed the default amount parallel of linker jobs to 2. **If you run out of RAM, close some memory hungry applications and re-run the command.** See `host-llvm` in [bootstrap.yml](../../bootstrap.yml)

## Build and run
Run `xbstrap install --all`, or `xbstrap install lemon-base` to only install the core applications and libraries.
```sh
xbstrap install --all # Build and install everything

OR

xbstrap install lemon-base # Build and install base applications and libraries
```

To build the disk image (requires root privileges) run
```sh
xbstrap run build-disk
```

### Running with QEMU/KVM
To run with QEMU run `Scripts/run.sh`
```sh
Scripts/run.sh
```

### Running with VirtualBox
To run with VirtualBox create a vm called "LemonOS". You'll need to symlink or rename `Disks/Lemon.img` to `Disks/Lemon.hdd` as VirtualBox cannot use `.img` files.
```sh
Scripts/run.sh vbox
```
