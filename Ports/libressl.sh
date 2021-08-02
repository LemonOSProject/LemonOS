unpack(){
    if ! [ -f cache/libressl-3.3.1.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/libressl-3.3.1.tar.gz "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.3.1.tar.gz"
    fi

 	tar -xzvf cache/libressl-3.3.1.tar.gz
 	export BUILD_DIR=libressl-3.3.1
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-libressl-3.3.1.patch
	mkdir build
	cd build

	cmake -G Ninja .. -DCMAKE_TOOLCHAIN_FILE=../../../Scripts/CMake/lemon-cmake-options.txt -DCMAKE_INSTALL_PREFIX=/system -DBUILD_SHARED_LIBS=ON -DLIBRESSL_APPS=OFF -DHAVE_TIMEGM=OFF

 	ninja
	DESTDIR=$LEMON_SYSROOT ninja install

	mkdir -p $LEMON_SYSROOT/system/lib/crypto
	ln -sf ../libcrypto.so $LEMON_SYSROOT/system/lib/crypto/libcrypto.so
}
