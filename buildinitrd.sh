ninja -C Applications/build
ninja -C Applications/build install

cd InitrdWriter && ./copy.sh
cp ../InitrdWriter/initrd-x86_64.img ../initrd.img

cd ..

objcopy -B i386 -I binary -O elf64-x86-64 initrd.img initrd.o
ld -T initrd.ld -relocatable initrd.o -o Kernel/initrd.o