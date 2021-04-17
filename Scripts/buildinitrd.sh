cp Resources/* Initrd/
cp -L ~/.local/share/lemon/sysroot/system/lib/libc.so* ~/.local/share/lemon/sysroot/system/lib/libc++*.so* ~/.local/share/lemon/sysroot/system/lib/libunwind.so* ~/.local/share/lemon/sysroot/system/lib/ld.so* ~/.local/share/lemon/sysroot/system/lib/libfreetype.so* Initrd/ # Only copy crucial libraries
cp Applications/build/lsh.lef Initrd/ # Create a backup of LSh on the ramdisk for FTerm
cp Applications/build/subprojects/LemonUtils/*.lef Initrd/ # Create a backup of LemonUtils on the ramdisk for FTerm

mkdir -p Initrd/modules
cp -r Kernel/build/Modules/*.sys Initrd/modules # Add kernel modules
cp Kernel/modules.cfg Initrd/

nm Kernel/build/kernel.sys > Initrd/kernel.map

cd Initrd
tar -cf ../initrd.tar *
cd ..