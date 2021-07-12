unpack(){
 	git clone --branch R_2_4_1 https://github.com/libexpat/libexpat.git libexpat --depth 1
 	export BUILD_DIR=libexpat
}

buildp(){
 	cd $BUILD_DIR

 	mkdir -p build
    cd build
    cmake ../expat -GNinja -DCMAKE_TOOLCHAIN_FILE=$LEMON_BUILDROOT/Scripts/CMake/lemon-cmake-options.txt -DCMAKE_INSTALL_PREFIX=/system
    ninja -j$JOBCOUNT
    DESTDIR=$LEMON_SYSROOT ninja install
}
