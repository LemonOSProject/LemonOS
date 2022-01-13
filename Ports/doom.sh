#!/bin/sh
unpack(){
 	git clone https://github.com/LemonOSProject/LemonDOOM.git --depth 1 LemonDOOM
 	export BUILD_DIR=LemonDOOM/lemondoom
}
 
buildp(){
 	cd $BUILD_DIR
 	make -f Makefile.lemon -j$JOBCOUNT
 	cp doom.lef "$LEMON_SYSROOT/$LEMON_PREFIX/bin"

	mkdir -p "$LEMON_SYSROOT/system/doom"
	curl -Lo "$LEMON_SYSROOT/system/doom/doom1.wad" "http://distro.ibiblio.org/pub/linux/distributions/slitaz/sources/packages/d/doom1.wad"
}
