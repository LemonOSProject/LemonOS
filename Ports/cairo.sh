unpack(){
    if ! [ -f cache/cairo-1.16.0.tar.xz ]; then # Check if tarball exists
        curl -Lo cache/cairo-1.16.0.tar.xz "https://www.cairographics.org/releases/cairo-1.16.0.tar.xz"
    fi
    tar -xJf cache/cairo-1.16.0.tar.xz
    export BUILD_DIR=cairo-1.16.0
}

buildp(){
 	cd $BUILD_DIR

 	patch -p1 < ../lemon-cairo-1.16.0.patch
    ./configure --host=x86_64-lemon --prefix=/system --disable-xlib --disable-xcb --without-x --disable-gobject --enable-svg=no --enable-pdf=no --disable-gtk-doc-html

 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
