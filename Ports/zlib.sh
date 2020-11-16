unpack(){
 	wget "https://zlib.net/zlib-1.2.11.tar.gz"
 	tar -xzvf zlib-1.2.11.tar.gz
 	export BUILD_DIR=zlib-1.2.11
 	rm zlib-1.2.11.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	./configure --prefix=$LEMON_PREFIX
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
