#!/bin/sh

set -e 1

sudo mkdir /mnt/Lemon

LOOPBACK_DEVICE=$(losetup --find --partscan --show Disks/Lemon.img)

sudo mount ${LOOPBACK_DEVICE}p2 /mnt/Lemon