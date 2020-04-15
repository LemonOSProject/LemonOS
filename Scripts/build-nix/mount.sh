#!/bin/bash

sudo mkdir /mnt/Lemon
sudo qemu-nbd -c /dev/nbd0 Disks/Lemon.vhd
sudo mount /dev/nbd0p2 /mnt/Lemon
