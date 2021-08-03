unpack(){
    if ! [ -f cache/busybox-1.33.1.tar.bz2 ]; then # Check if tarball exists
 		curl -Lo cache/busybox-1.33.1.tar.bz2 "https://www.busybox.net/downloads/busybox-1.33.1.tar.bz2"
	fi

 	tar -xjvf cache/busybox-1.33.1.tar.bz2
 	export BUILD_DIR=busybox-1.33.1
}

buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-busybox-1.33.1.patch
    cp configs/lemon_defconfig .config

 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
 	cp -r ./_install/bin/* "$LEMON_SYSROOT/system/bin"
}
