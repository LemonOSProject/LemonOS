LOOPBACK_DEVICE=$(losetup --find --partscan --show Disks/Lemon.img) # Find an empty loopback device and mount
echo "Mounted image as loopback device at ${LOOPBACK_DEVICE}"

cleanup1(){
    losetup -d "${LOOPBACK_DEVICE}"
}
trap 'cleanup1' 1

echo "Formatting ${LOOKBACK_DEVICE}p2 as FAT32"
mkfs.vfat -F 32 "${LOOPBACK_DEVICE}"p2

echo "Formatting ${LOOKBACK_DEVICE}p3 as ext2"
mkfs.ext2 -b 4096 "${LOOPBACK_DEVICE}"p3

mkdir -p /mnt/Lemon
mkdir -p /mnt/LemonEFI
mount "${LOOPBACK_DEVICE}"p2 /mnt/Lemon
mount "${LOOPBACK_DEVICE}"p3 /mnt/LemonEFI

cleanup2(){
    umount -f /mnt/Lemon
    umount -f /mnt/LemonEFI

    cleanup1

    rmdir /mnt/Lemon
    rmdir /mnt/LemonEFI
}
trap 'cleanup2' 1

mkdir -p /mnt/LemonEFI/EFI/BOOT
if [ ! -z "$USE_GRUB_EFI" ]; then
    grub-install --target=x86_64-efi --boot-directory=/mnt/Lemon/lemon/boot --efi-directory=/mnt/LemonEFI "${LOOPBACK_DEVICE}" --removable
elif [ -e "Build/tools/host-limine/share/limine/BOOTX64.EFI" ]; then
    cp "Build/tools/host-limine/share/limine/BOOTX64.EFI" /mnt/LemonEFI/EFI/BOOT/
    cp "Build/tools/host-limine/share/limine/limine.sys" /mnt/Lemon/
else
    echo "Failed to find limine BOOTX64.EFI or limine.sys"
    cleanup2
    exit 1
fi

echo ".\EFI\BOOT\BOOTX64.EFI" > /mnt/LemonEFI/startup.nsh

mkdir -p /mnt/Lemon/lemon/boot

umount /mnt/Lemon
umount /mnt/LemonEFI

#limine-deploy "Disks/Lemon.img" 1

losetup -d "${LOOPBACK_DEVICE}"

rmdir /mnt/Lemon
rmdir /mnt/LemonEFI
