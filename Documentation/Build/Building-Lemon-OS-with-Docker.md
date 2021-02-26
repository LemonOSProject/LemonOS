## Prerequisites
* A UNIX-like build system
* Docker
* qemu-img

Install the following (Arch) packages:\
`git`, `docker`, `qemu`\
Or Debian, Ubuntu, etc.:\
`git`, `docker.io`, `qemu-utils`
For building the disk image:\
`grub-pc`, `grub-efi-amd64-bin`, `e2fsprogs`, `dosfstools`

## Cloning
Make sure you use `--recurse-submodules` to get the submodules
`git clone https://github.com/LemonOSProject/LemonOS.git --recurse-submodules`

## Configuration
Run ```Scripts/docker-configure.sh``` this will do the following:
* Pull the docker image
* Configure the meson projects
* Build most of the various ports

## Creating/Obtaining disk image
Install the GRUB package for your distro and run `Scripts/createdisk.sh`. This will create a disk image with limine as BIOS bootloader and GRUB for EFI

## Finally build and run
In the main directory run `./docker-make all` to build everything. Finally `./docker-make run` to run Lemon OS with QEMU/KVM, or run Disks/Lemon.img in your favourite VM.
