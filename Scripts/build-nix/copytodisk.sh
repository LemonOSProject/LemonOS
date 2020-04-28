#!/bin/bash
sudo Scripts/build-nix/mount.sh 
sudo cp Kernel/build/kernel.sys /mnt/Lemon/Lemon/kernel64.sys
sudo Scripts/build-nix/unmount.sh
