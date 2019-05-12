nasm -f elf32 -o init.o init.asm
i686-elf-gcc -m32 -fno-permissive -fno-exceptions -fno-rtti -nostdlib -ffreestanding -I../../LibC/include -I../../LibLemon/include -L../../Libraries -o init.lef init.o main.cpp -lc -llemon
