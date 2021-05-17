unpack(){
 	wget "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.3.1.tar.gz"
 	tar -xzvf libressl-3.3.1.tar.gz
 	export BUILD_DIR=libressl-3.3.1
 	rm libressl-3.3.1.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-libressl-3.3.1.patch
	mkdir build
	cd build

	cmake -G Ninja .. -DCMAKE_TOOLCHAIN_FILE=../../../Scripts/CMake/lemon-cmake-options.txt -DCMAKE_INSTALL_PREFIX=/system -DBUILD_SHARED_LIBS=ON -DLIBRESSL_APPS=OFF

 	ninja
	DESTDIR=$LEMON_SYSROOT ninja install
}
