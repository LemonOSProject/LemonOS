if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

cd "$LEMOND"

cp Resources/* Initrd/
cp -L ~/.local/share/lemon/sysroot/system/lib/libc.so* ~/.local/share/lemon/sysroot/system/lib/libc++*.so* ~/.local/share/lemon/sysroot/system/lib/libunwind.so* ~/.local/share/lemon/sysroot/system/lib/ld.so* ~/.local/share/lemon/sysroot/system/lib/libfreetype.so* ~/.local/share/lemon/sysroot/system/lib/libpthread.so* Initrd/ # Only copy crucial libraries
cp Build/Applications/lsh.lef Initrd/ # Create a backup of LSh on the ramdisk for FTerm
cp Build/subprojects/LemonUtils/*.lef Initrd/ # Create a backup of LemonUtils on the ramdisk for FTerm

mkdir -p Initrd/modules
cp -r Build/Kernel/Modules/*.sys Initrd/modules # Add kernel modules
cp Kernel/modules.cfg Initrd/

nm Build/Kernel/kernel.sys > Initrd/kernel.map

cd Initrd
tar -cf ../initrd.tar *
cd ..