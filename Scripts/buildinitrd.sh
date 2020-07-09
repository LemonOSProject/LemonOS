ninja -C Applications/build install

cp Resources/* Initrd/
cp ~/.local/share/lemon/sysroot/system/lib/*.so* Initrd/

cd Initrd
tar -cf ../initrd.tar *
cd ..
