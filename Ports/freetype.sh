unpack(){
 	wget "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.1.tar.gz"
 	tar -xzvf freetype-2.10.1.tar.gz
 	export BUILD_DIR=freetype-2.10.1
 	rm freetype-2.10.1.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-freetype-2.10.1.patch
 	export CC=x86_64-lemon-gcc
 	./configure --host=x86_64-lemon --prefix=$LEMON_SYSROOT --with-harfbuzz=no --with-bzip2=no --disable-mmap --without-zlib --without-png
 	make -j$JOBCOUNT
 	make install
}
