unpack(){
 	wget "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.4.tar.gz"
 	tar -xzvf freetype-2.10.4.tar.gz
 	export BUILD_DIR=freetype-2.10.4
 	rm freetype-2.10.4.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-freetype-2.10.4.patch
 	./configure --host=x86_64-lemon --prefix=$LEMON_PREFIX --with-harfbuzz=no --with-bzip2=no --disable-mmap --with-zlib=no --with-png=no --enable-shared --with-brotli=no
 	make $JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
	ln -sf freetype2/freetype $LEMON_SYSROOT/$LEMON_PREFIX/include/
	ln -sf freetype2/ft2build.h $LEMON_SYSROOT/$LEMON_PREFIX/include/
}
