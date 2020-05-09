mkdir -p InitrdWriter/Contents

ninja -C Applications/build install

cp Resources/* Initrd/
cp ~/.local/share/lemon/sysroot/lib/*.so* Initrd/

cd Initrd
tar -cvf ../initrd.tar *
cd ..