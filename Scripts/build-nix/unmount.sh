 #!/bin/bash

sudo umount /mnt/Lemon
sudo rmdir /mnt/Lemon
sudo qemu-nbd -d /dev/nbd0
