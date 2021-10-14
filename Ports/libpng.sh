unpack(){
    if ! [ -f cache/libpng-1.6.37.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/libpng-1.6.37.tar.gz "https://download.sourceforge.net/libpng/libpng-1.6.37.tar.gz"
    fi
	
 	tar -xzf cache/libpng-1.6.37.tar.gz
 	export BUILD_DIR=libpng-1.6.37
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-libpng-1.6.37.patch
 	./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon --enable-shared
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
