unpack(){
    if ! [ -f cache/mesa-20.3.4.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/mesa-20.3.4.tar.gz "https://github.com/mesa3d/mesa/archive/mesa-20.3.4.tar.gz"
    fi

 	tar -xzf cache/mesa-20.3.4.tar.gz
 	export BUILD_DIR=mesa-mesa-20.3.4
}
 
buildp(){
	cd $BUILD_DIR
 	patch -p1 < ../lemon-mesa-20.3.4.patch

	meson build --cross ../../Scripts/lemon-crossfile.txt -Dglx=disabled -Ddri=disabled  

	cd build
	ninja
	ninja install
}
