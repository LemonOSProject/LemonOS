#!/bin/sh
set -e

LOOPBACK_DEVICE=""
DEVICE=""

sudo mkdir -p /mnt/Lemon

if [ -z $1 ]; then
	LOOPBACK_DEVICE=$(sudo losetup --find --partscan --show Disks/Lemon.img)
	DEVICE="${LOOPBACK_DEVICE}"p2
else
	DEVICE=$1
fi

echo "Mounting ${DEVICE} on /mnt/Lemon..."
sudo mount $DEVICE /mnt/Lemon

sudo cp initrd.tar /mnt/Lemon/lemon/initrd.tar
sudo cp Kernel/build/kernel.sys /mnt/Lemon/lemon/kernel.sys
sudo cp -ru $HOME/.local/share/lemon/sysroot/system/* /mnt/Lemon
sudo cp -ru Base/* /mnt/Lemon/

echo "Unmounting /mnt/Lemon..."
sudo umount /mnt/Lemon

if [ -z $1 ]; then
	sudo losetup -d ${LOOPBACK_DEVICE}
fi

sudo rmdir /mnt/Lemon
