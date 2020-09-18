unpack(){
 	git clone "https://github.com/klange/nyancat" nyancat
 	export BUILD_DIR=nyancat
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-nyancat.patch
 	export CC=x86_64-lemon-gcc
 	make -j$JOBCOUNT
	cp src/nyancat $LEMON_SYSROOT/$LEMON_PREFIX/bin/nyancat.lef
}
