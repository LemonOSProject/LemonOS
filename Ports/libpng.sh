unpack(){
 	wget "https://download.sourceforge.net/libpng/libpng-1.6.37.tar.gz"
 	tar -xzvf libpng-1.6.37.tar.gz
 	export BUILD_DIR=libpng-1.6.37
 	rm libpng-1.6.37.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-libpng-1.6.37.patch
 	./configure --prefix=$LEMON_SYSROOT --host=x86_64-lemon
 	make -j$JOBCOUNT
 	make install
}
