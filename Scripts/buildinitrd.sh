mkdir -p InitrdWriter/Contents

ninja -C Applications/build install

cp Resources/* Initrd/
cp ~/.local/share/lemon/sysroot/lib/*.so* Initrd/
cp -r ~/.local/share/lemon/sysroot/bin Initrd/

cd Initrd
tar -cf ../initrd.tar *
cd ..