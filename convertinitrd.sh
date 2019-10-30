objcopy -B i386 -I binary -O elf64-x86-64 initrd.img initrd.o
ld -T initrd.ld -relocatable initrd.o -o Kernel/obj/x86_64/initrd.o

