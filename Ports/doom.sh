unpack(){
 	git clone https://github.com/LemonOSProject/LemonDOOM.git --depth 1 LemonDOOM
 	export BUILD_DIR=LemonDOOM/lemondoom
}
 
buildp(){
 	cd $BUILD_DIR
 	make -f Makefile.lemon -j$JOBCOUNT
 	cp doom.lef "$LEMON_SYSROOT/$LEMON_PREFIX/bin"
}
