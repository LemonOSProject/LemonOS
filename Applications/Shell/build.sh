nasm -f elf32 -o shell.o shell.asm
i686-elf-gcc -m32 -fno-permissive -fno-exceptions -fno-rtti -nostdlib -ffreestanding -L../../Libraries -I../../LibC/include -I../../LibLemon/include -o shell.lef shell.o main.cpp -lc -llemon
