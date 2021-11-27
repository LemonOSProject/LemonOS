unpack(){
 	git clone https://github.com/Butterzz1288/ButterTools.git --depth 1 lemon-BTools
 	export BUILD_DIR=ButterTools-3.1
}
 
buildp(){
 	cd $BUILD_DIR

	./configure --host=x86_64-lemon
 	make -j$JOBCOUNT
 	cp lemonbtools "$LEMON_SYSROOT/$LEMON_PREFIX/bin/.lef"
}
