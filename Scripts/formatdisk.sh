set -e

LOOPBACK_DEVICE=$(losetup --find --partscan --show Disks/Lemon.img) # Find an empty loopback device and mount
echo "Mounted image as loopback device at ${LOOPBACK_DEVICE}"

echo "Formatting ${LOOKBACK_DEVICE}p2 as ext2"
mkfs.ext2 -b 4096 "${LOOPBACK_DEVICE}"p2

echo "Formatting ${LOOKBACK_DEVICE}p3 as FAT32"
mkfs.vfat -F 32 "${LOOPBACK_DEVICE}"p3

mkdir -p /mnt/Lemon
mkdir -p /mnt/LemonEFI
mount "${LOOPBACK_DEVICE}"p2 /mnt/Lemon
mount "${LOOPBACK_DEVICE}"p3 /mnt/LemonEFI

mkdir -p /mnt/Lemon/lemon/boot

grub-install --target=x86_64-efi --boot-directory=/mnt/Lemon/lemon/boot --efi-directory=/mnt/LemonEFI "${LOOPBACK_DEVICE}" --removable

umount /mnt/Lemon
umount /mnt/LemonEFI

if [ -x "$(command -v limine-install)" ]; then
    limine-install "${LOOPBACK_DEVICE}"
else
    export PATH=$PATH:$HOME/.local/share/lemon/bin
    
    if [ -x "$(command -v limine-install)" ]; then
        limine-install "${LOOPBACK_DEVICE}"
    elif [ -e "Toolchain/limine-1.0/limine-install" ]; then
        Toolchain/limine-1.0/limine-install "${LOOPBACK_DEVICE}"
    else
        echo "Failed to find limine-install!"
        exit 1
    fi
fi

losetup -d "${LOOPBACK_DEVICE}"

rmdir /mnt/Lemon
rmdir /mnt/LemonEFI