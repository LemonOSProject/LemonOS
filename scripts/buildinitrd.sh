#!/bin/sh

if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

cd "$LEMOND"

set -e

mkdir -p Initrd/
cp -ru Resources/* Initrd/
cp -L Build//sysroot/system/lib/libc.so* Build//sysroot/system/lib/libc++*.so* Build//sysroot/system/lib/libunwind.so* Build//sysroot/system/lib/ld.so* Build//sysroot/system/lib/libfreetype.so* Build//sysroot/system/lib/libpthread.so* Build//sysroot/system/lib/librt.so* Build//sysroot/system/lib/libdl.so* Initrd/ # Only copy crucial libraries
cp Build/sysroot/system/bin/lsh Initrd/ # Create a backup of LSh on the ramdisk for FTerm
cp Build/packages/lemon-utils/system/bin/* Initrd/ # Create a backup of LemonUtils on the ramdisk for FTerm

mkdir -p Initrd/modules
cp -r Build/pkg-builds/lemon-kernel/Modules/*.sys Initrd/modules # Add kernel modules
cp Kernel/modules.cfg Initrd/

nm Build/packages/lemon-kernel/system/lemon/kernel.sys > Initrd/kernel.map

cd Initrd
tar -cf ../Build/sysroot/system/lemon/initrd.tar *
cd ..
