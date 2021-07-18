#!/bin/sh

if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

if [ ! -f "$LEMOND/Disks/Lemon.img" ]; then
	$LEMOND/Scripts/createdisk.sh
fi

LOOPBACK_DEVICE=""
DEVICE=""

if [ -z $1 ]; then
	LOOPBACK_DEVICE=$(sudo mkdir -p /mnt/Lemon; sudo losetup --find --partscan --show "$LEMOND/Disks/Lemon.img")
	DEVICE="${LOOPBACK_DEVICE}"p2
else
	DEVICE=$1
fi

cleanup(){
	sudo umount /mnt/Lemon

	if [ -z $1 ]; then
		sudo losetup -d ${LOOPBACK_DEVICE}
	fi
}

trap 'cleanup' 1

$LEMOND/Scripts/buildinitrd.sh

echo "Mounting ${DEVICE} on /mnt/Lemon..."
sudo sh -c "mount $DEVICE /mnt/Lemon; chown -R $USER /mnt/Lemon"

cp -ru $HOME/.local/share/lemon/sysroot/system/* /mnt/Lemon
cp "$LEMOND/initrd.tar" /mnt/Lemon/lemon/initrd.tar
cp "$LEMOND/Build/Kernel/kernel.sys" /mnt/Lemon/lemon/kernel.sys
cp -ru "$LEMOND/Base/"* /mnt/Lemon/

echo "Unmounting /mnt/Lemon..."
sudo sh -c "umount /mnt/Lemon;rmdir /mnt/Lemon"

if [ -z $1 ]; then
	sudo losetup -d ${LOOPBACK_DEVICE}
fi
