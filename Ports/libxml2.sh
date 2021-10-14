unpack(){
    if ! [ -f cache/libxml2-v2.9.11.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/libxml2-v2.9.11.tar.gz "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.9.11/libxml2-v2.9.11.tar.gz"
    fi

 	tar -xzf cache/libxml2-v2.9.11.tar.gz
 	export BUILD_DIR=libxml2-v2.9.11
}
 
buildp(){
 	cd $BUILD_DIR
	
	mkdir build
	cd build

	cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=$LEMON_BUILDROOT/Scripts/CMake/lemon-cmake-options.txt -DCMAKE_INSTALL_PREFIX=/system -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_ICU=ON -DLIBXML2_WITH_PYTHON=OFF
	ninja
	DESTDIR=$LEMON_SYSROOT ninja install
}
