ninja -C Applications/build install

cp Resources/* Initrd/
cp ~/.local/share/lemon/sysroot/system/lib/*.so* Initrd/
cp Applications/build/lsh.lef Initrd/ # Create a backup of LSh on the ramdisk for FTerm
cp Applications/build/subprojects/LemonUtils/*.lef Initrd/ # Create a backup of LemonUtils on the ramdisk for FTerm

cd Initrd
tar -cf ../initrd.tar *
cd ..
