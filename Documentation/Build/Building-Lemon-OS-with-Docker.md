# Building Lemon OS with Docker

## Prerequisites
* A UNIX-like build system
* Docker

Install the following (Arch) packages:\
`git`, `docker`\
Or Debian, Ubuntu, etc.:\
`git`, `docker.io`\
For building the disk image:\
`e2fsprogs`, `dosfstools`

## Cloning
Make sure you use `--recurse-submodules` to get the submodules
`git clone https://github.com/LemonOSProject/LemonOS.git --recurse-submodules`

## Configuration
Run ```Scripts/docker-configure.sh``` this will do the following:
* Pull the docker image
* Configure the meson projects
* Build the essential ports

## Build the system
Run ```Scripts/docker-run.sh ninja -C Build install```

## Creating/Obtaining disk image
Run `Scripts/createdisk.sh`. \
This will create `Disks/Lemon.img` with limine as BIOS and EFI bootloader if it does not exist. Then it will copy the built system files to the disk. For now this will have to be run on the host system.

## Running
[Refer to Building Lemon OS](Building-Lemon-OS.md)

## Building Ports
Run
```sh
Scripts/docker-run.sh Ports/buildport.sh <port name>  # e.g. ./buildport.sh nyancat
```
