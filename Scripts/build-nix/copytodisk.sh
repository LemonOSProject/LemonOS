#!/bin/bash
sudo Scripts/build-nix/mount.sh 
sudo cp initrd.tar /mnt/Lemon/Lemon/initrd.tar
sudo cp Kernel/build/kernel.sys /mnt/Lemon/Lemon/kernel.sys
sudo cp -r Base/* /mnt/Lemon/
sudo Scripts/build-nix/unmount.sh
