unpack(){
 	git clone https://github.com/LemonOSProject/lemon-gnuboy.git --depth 1 lemon-gnuboy
 	export BUILD_DIR=lemon-gnuboy
}
 
buildp(){
 	cd $BUILD_DIR

	./configure --host=x86_64-lemon
 	make -j$JOBCOUNT
 	cp lemongnuboy "$LEMON_SYSROOT/$LEMON_PREFIX/bin/gnuboy.lef"
}
