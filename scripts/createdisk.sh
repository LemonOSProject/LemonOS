#!/bin/sh

SPATH=$(dirname $(readlink -f "$0"))

set -e 1

cd $SPATH/..
export LEMONDIR=$(pwd)

mkdir -p Disks

dd if=/dev/zero of=build/lemon.img bs=512 count=6291456
sfdisk build/lemon.img < scripts/partitions.sfdisk

echo "Formatting disk!"
sudo scripts/formatdisk.sh
