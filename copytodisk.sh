#!/bin/bash
sudo ./mount.sh 
sudo cp Kernel/bin/x86/kernel.sys /mnt/Lemon/Lemon/kernel.sys
sudo cp Kernel/bin/x86_64/kernel.sys /mnt/Lemon/Lemon/kernel64.sys
sudo cp initrd.img /mnt/Lemon/Lemon/initrd.img
sudo ./unmount.sh
