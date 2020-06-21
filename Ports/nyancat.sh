unpack(){
 	git clone "https://github.com/klange/nyancat" nyancat
 	export BUILD_DIR=nyancat
}
 
buildp(){
 	cd $BUILD_DIR
 	export CC=x86_64-lemon-gcc
 	make -j$JOBCOUNT
	cp src/nyancat $LEMON_SYSROOT/bin/nyancat.lef
}
