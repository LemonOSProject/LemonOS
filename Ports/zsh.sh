unpack(){
    if ! [ -f cache/zsh-5.8.tar.xz ]; then # Check if tarball exists
 		curl -Lo cache/zsh-5.8.tar.xz "https://www.zsh.org/pub/zsh-5.8.tar.xz"
	fi

 	tar -xJf cache/zsh-5.8.tar.xz
 	export BUILD_DIR=zsh-5.8
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-zsh-5.8.patch
 	./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon --disable-gdbm --disable-locale --with-term-lib="tinfo termcap ncurses" --disable-multibyte
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
