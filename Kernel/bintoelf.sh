#!/bin/sh
nasm -fbin -i "$3" "$1" -o "$4.bin"
objcopy -I binary -O elf64-x86-64 -B i386 "$4.bin" $2