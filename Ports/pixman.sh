unpack(){
    if ! [ -f cache/pixman-0.40.0.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/pixman-0.40.0.tar.gz "https://www.cairographics.org/releases/pixman-0.40.0.tar.gz"
    fi
    tar -xzvf cache/pixman-0.40.0.tar.gz
    export BUILD_DIR=pixman-0.40.0
}

buildp(){
 	cd $BUILD_DIR

 	patch -p1 < ../lemon-pixman-0.40.0.patch
    ./configure --host=x86_64-lemon --prefix=/system

 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
