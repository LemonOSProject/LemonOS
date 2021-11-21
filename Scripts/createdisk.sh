#!/bin/sh

SPATH=$(dirname $(readlink -f "$0"))

set -e 1

cd $SPATH/..
export LEMONDIR=$(pwd)

mkdir -p Disks

dd if=/dev/zero of=Disks/Lemon.img bs=512 count=4194304
sfdisk Disks/Lemon.img < Scripts/partitions.sfdisk

echo "Formatting disk! your files will be deleted tho 🙁"
sudo Scripts/formatdisk.sh
