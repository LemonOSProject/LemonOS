 #!/bin/bash

sudo umount /mnt/Lemon
sudo rm -rf /mnt/Lemon
sudo qemu-nbd -d /dev/nbd0
