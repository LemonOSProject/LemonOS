unpack(){
    if ! [ -f cache/libedit-20191231-3.1.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/libedit-20191231-3.1.tar.gz "https://thrysoee.dk/editline/libedit-20191231-3.1.tar.gz"
    fi
 	tar -xzf cache/libedit-20191231-3.1.tar.gz
    export BUILD_DIR=libedit-20191231-3.1
}
 
buildp(){
 	cd $BUILD_DIR
    patch -p1 < ../lemon-libedit-20191231-3.1.patch
    CFLAGS="$CFLAGS -I$LEMON_SYSROOT/$LEMON_PREFIX/include/ncurses" ac_cv_func_getrlimit=no ./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon --with-tlib=ncurses
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
