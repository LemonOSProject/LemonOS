#!/bin/sh
unpack(){
 	git clone "https://github.com/klange/nyancat" nyancat --depth 1
 	export BUILD_DIR=nyancat
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-nyancat.patch
 	make -j$JOBCOUNT
	cp src/nyancat $LEMON_SYSROOT/$LEMON_PREFIX/bin/nyancat
}
