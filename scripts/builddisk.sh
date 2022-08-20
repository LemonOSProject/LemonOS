#!/bin/sh

if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

LOOPBACK_DEVICE=""
DEVICE=""

if [ -z $1 ]; then
	if [ ! -f "$LEMOND/build/lemon.img" ]; then
		$LEMOND/scripts/createdisk.sh
	fi

	LOOPBACK_DEVICE=$(sudo mkdir -p /mnt/Lemon; sudo losetup --find --partscan --show "$LEMOND/build/lemon.img")
	DEVICE="${LOOPBACK_DEVICE}"p3
else
	DEVICE=$1
	sudo mkdir -p /mnt/Lemon
fi

cleanup(){
	sudo umount /mnt/Lemon

	if [ -z $1 ]; then
		sudo losetup -d ${LOOPBACK_DEVICE}
	fi
}

trap 'cleanup' 1

echo "Mounting ${DEVICE} on /mnt/Lemon..."
sudo sh -c "mount $DEVICE /mnt/Lemon; chown -R $USER /mnt/Lemon"

cp -rau "$LEMOND/build/sysroot/system/." /mnt/Lemon

echo "Unmounting /mnt/Lemon..."
sudo sh -c "umount /mnt/Lemon;rmdir /mnt/Lemon"

if [ -z $1 ]; then
	sudo losetup -d ${LOOPBACK_DEVICE}
fi
