unpack(){
 	wget "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.2.tar.gz"
 	tar -xzvf ncurses-6.2.tar.gz
 	export BUILD_DIR=ncurses-6.2
 	rm ncurses-6.2.tar.gz
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-ncurses-6.2.patch
 	./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon --with-termlib --without-ada --with-shared --with-cxx-shared
 	CFLAGS="$CFLAGS -fPIC" CXXFLAGS=$CFLAGS make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
