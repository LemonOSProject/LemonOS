unpack(){
    if ! [ -f cache/zlib-1.2.11.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/zlib-1.2.11.tar.gz "https://zlib.net/zlib-1.2.11.tar.gz"
    fi

 	tar -xzvf cache/zlib-1.2.11.tar.gz
 	export BUILD_DIR=zlib-1.2.11
}
 
buildp(){
 	cd $BUILD_DIR
 	./configure --prefix=$LEMON_PREFIX
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
