#!/bin/bash

set -e

cd "$LEMON_BUILDROOT/toolchain"

if [ ! -d limine ]; then
    git clone https://github.com/limine-bootloader/limine.git --branch v5.x-branch --depth 1 limine
fi

cd limine

./bootstrap
./configure --enable-uefi-x86-64 --enable-bios --enable-bios-pxe --enable-bios-cd --enable-uefi-cd

make $MAKE_FLAGS

if [ ! -x bin/limine ]; then
    echo "Could not find limine executable"
    exit 1
fi
