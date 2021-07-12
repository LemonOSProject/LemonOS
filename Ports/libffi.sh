unpack(){
 	git clone --branch v3.4.2 https://github.com/libffi/libffi.git libffi --depth 1
 	export BUILD_DIR=libffi
}

buildp(){
 	cd $BUILD_DIR
    patch -p1 < ../lemon-libffi-3.4.2.patch

    ./autogen.sh
 	./configure --host=x86_64-lemon --prefix=$LEMON_PREFIX --with-pic

 	make $JOBCOUNT
 	DESTDIR=$LEMON_SYSROOT make install
}
