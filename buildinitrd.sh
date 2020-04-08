cd Applications/Init && ./build.sh && cp init.lef ../../InitrdWriter/Contents
cd ../FileManager && ./build.sh && cp fileman.lef ../../InitrdWriter/Contents
cd ../Snake && ./build.sh && cp snake.lef ../../InitrdWriter/Contents
cd ../Shell && ./build.sh && cp shell.lef ../../InitrdWriter/Contents

cd ../../InitrdWriter && ./copy.sh
cp ../InitrdWriter/initrd-x86_64.img ../initrd.img

cd ..

objcopy -B i386 -I binary -O elf64-x86-64 initrd.img initrd.o
ld -T initrd.ld -relocatable initrd.o -o Kernel/initrd.o