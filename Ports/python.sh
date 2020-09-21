unpack(){
    wget "https://www.python.org/ftp/python/3.8.2/Python-3.8.2.tgz"
 	tar -xzvf Python-3.8.2.tgz
 	export BUILD_DIR=Python-3.8.2
 	rm Python-3.8.2.tgz
}

buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-Python-3.8.2.patch
 	
	autoconf
    ac_cv_file__dev_ptmx=yes ac_cv_file__dev_ptc=no ./configure --host=x86_64-lemon --disable-ipv6 --build=x86_64 --prefix=/system --without-ensurepip --with-sysroot=$LEMON_SYSROOT
    
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
