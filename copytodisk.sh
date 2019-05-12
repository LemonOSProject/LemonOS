#!/bin/bash
sudo ./mount.sh 
sudo cp Kernel/bin/x86/kernel.sys /mnt/Lemon/Lemon/kernel.sys
sudo cp initrd.img /mnt/Lemon/Lemon/initrd.img
sudo ./unmount.sh