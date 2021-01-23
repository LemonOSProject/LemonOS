#!/bin/sh

SPATH=$(dirname $(readlink -f "$0"))

set -e

cd $SPATH/..
export LEMONDIR=$(pwd)

mkdir -p Disks
qemu-img create -f vpc Disks/Lemon.vhd 1048072K

sudo Scripts/partitiondisk.sh