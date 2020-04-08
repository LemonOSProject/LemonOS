#!/bin/bash
sudo ./mount.sh 
sudo cp ../../Kernel/build/kernel.sys /mnt/Lemon/Lemon/kernel64.sys
sudo cp ../../initrd.img /mnt/Lemon/Lemon/initrd.img
sudo ./unmount.sh
