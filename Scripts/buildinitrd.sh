mkdir -p InitrdWriter/Contents

ninja -C Applications/build
ninja -C Applications/build install

cp Resources/* InitrdWriter/Contents/
cp ~/.local/share/lemon/sysroot/usr/lib/*.so* InitrdWriter/Contents/
cd InitrdWriter && ./copy.sh

objcopy -B i386 -I binary -O elf64-x86-64 initrd-x86_64.img initrd.o
ld -T initrd.ld -relocatable initrd.o -o ../Kernel/initrd.o