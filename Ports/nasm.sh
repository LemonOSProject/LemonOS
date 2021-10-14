unpack(){
    if ! [ -f cache/nasm-2.15.02.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/nasm-2.15.02.tar.gz "https://www.nasm.us/pub/nasm/releasebuilds/2.15.02/nasm-2.15.02.tar.gz"
    fi
	
 	tar -xzf cache/nasm-2.15.02.tar.gz
 	export BUILD_DIR=nasm-2.15.02
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-nasm-2.15.02.patch
 	ac_cv_func_getrlimit=no ./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
