nasm -f elf32 -o snake.o snake.asm
i686-elf-gcc -m32 -fno-permissive -fno-exceptions -fno-rtti -nostdlib -ffreestanding -L../../Libraries -I../../LibC/include -I../../LibLemon/include -o snake.lef snake.o main.cpp -lc -llemon
