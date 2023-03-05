#!/bin/sh

if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

cd "$LEMOND"

set -e

INITRDDIR="$LEMOND/build/initrd"

mkdir -p $INITRDDIR
mkdir -p $INITRDDIR/modules

cp -ru kernel/resources/* $INITRDDIR
cp -L build//sysroot/system/lib/libc.so* build//sysroot/system/lib/libc++*.so* build//sysroot/system/lib/libunwind.so* build//sysroot/system/lib/ld.so* build//sysroot/system/lib/libfreetype.so* build//sysroot/system/lib/libpthread.so* build//sysroot/system/lib/librt.so* build//sysroot/system/lib/libdl.so* $INITRDDIR # Only copy crucial libraries
cp build/sysroot/system/bin/lsh $INITRDDIR # Create a backup of LSh on the ramdisk for FTerm
cp $(find build/pkg-builds/lemon-system/utilities -maxdepth 1 -type f -not -name '*.*') $INITRDDIR # Create a backup of LemonUtils on the ramdisk for FTerm

cp -r build/pkg-builds/lemon-kernel/modules/*.sys $INITRDDIR/modules # Add kernel modules

cd $INITRDDIR
tar -cf $LEMOND/build/sysroot/system/lemon/initrd.tar *
cd $LEMOND
