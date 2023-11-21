#!/bin/bash

set -e

mkdir -p "$LEMON_BUILDROOT/build/iso_root"

cd $LEMON_BUILDROOT/build

cp -vu $LIMINE_BIN_DIR/{limine-bios.sys,limine-bios-cd.bin,limine-uefi-cd.bin} iso_root

cp -rvu ../base/* iso_root
cp -vu kernel.sys iso_root

mkdir -p iso_root/EFI/BOOT
cp -vu $LIMINE_BIN_DIR/BOOTX64.EFI iso_root/EFI/BOOT

# These options are a bit cryptic:
# -no-emul-boot Supposedly stops the BIOS from emulating a floppy disk
# -boot-load-size 4 is the number of blocks (512 byte) the BIOS loads from the boot image
xorriso -as mkisofs -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    -o lemon.iso iso_root \
