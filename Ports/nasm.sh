unpack(){
 	wget "https://www.nasm.us/pub/nasm/releasebuilds/2.15.02/nasm-2.15.02.tar.gz"
 	tar -xzvf nasm-2.15.02.tar.gz
 	export BUILD_DIR=nasm-2.15.02
 	rm nasm-2.15.02.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
