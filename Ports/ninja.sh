unpack(){
 	git clone https://github.com/LemonOSProject/ninja.git --depth 1 ninja
 	export BUILD_DIR=ninja
}
 
buildp(){
 	cd $BUILD_DIR

 	mkdir -p build
    cd build
    cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../../../Scripts/lemon-cmake-options.txt -DCMAKE_INSTALL_PREFIX=/system
    ninja -j$JOBCOUNT
    DESTDIR=$LEMON_SYSROOT ninja install
}
 