unpack(){
 	wget "https://zlib.net/zlib-1.2.11.tar.gz"
 	tar -xzvf zlib-1.2.11.tar.gz
 	export BUILD_DIR=zlib-1.2.11
 	rm zlib-1.2.11.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	export CC=x86_64-lemon-gcc
 	./configure --prefix=$LEMON_PREFIX --static
 	make -j$JOBCOUNT
 	make install
}
