nasm -f elf32 -o fm.o fm.asm
i686-elf-gcc -m32 -fno-permissive -fno-exceptions -fno-rtti -nostdlib -ffreestanding -L../../Libraries -I../../LibC/include -I../../LibLemon/include -o fm.lef fm.o main.cpp -llemon -lc 
