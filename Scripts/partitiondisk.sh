modprobe nbd

set -e 1

qemu-nbd -c /dev/nbd0 Disks/Lemon.vhd

sfdisk /dev/nbd0 < Scripts/partitions.sfdisk

partprobe /dev/nbd0

sleep 0.5

while [ ! -e /dev/nbd0p2 ]; do
	sleep 1 # Wait a second if does not exist

    partprobe /dev/nbd0

    sleep 0.5
done

mkfs.ext2 -b 4096 /dev/nbd0p2
mkfs.vfat -F 32 /dev/nbd0p3

mkdir -p /mnt/Lemon
mkdir -p /mnt/LemonEFI
mount /dev/nbd0p2 /mnt/Lemon
mount /dev/nbd0p3 /mnt/LemonEFI

mkdir -p /mnt/Lemon/lemon/boot

grub-install --target=x86_64-efi --boot-directory=/mnt/Lemon/lemon/boot --efi-directory=/mnt/LemonEFI /dev/nbd0 --removable
grub-install --target=i386-pc --boot-directory=/mnt/Lemon/lemon/boot /dev/nbd0

umount /mnt/Lemon
umount /mnt/LemonEFI

qemu-nbd -d /dev/nbd0 

rmdir /mnt/Lemon
rmdir /mnt/LemonEFI
