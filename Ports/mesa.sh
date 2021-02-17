

unpack(){
 	wget "https://github.com/mesa3d/mesa/archive/mesa-20.3.4.tar.gz"
 	tar -xzvf mesa-20.3.4.tar.gz
 	export BUILD_DIR=mesa-mesa-20.3.4
 	rm mesa-20.3.4.tar.gz
}
 
buildp(){
	cd $BUILD_DIR
 	patch -p1 < ../lemon-mesa-20.3.4.patch

	meson build --cross ../../Scripts/lemon-crossfile.txt -Dglx=disabled -Ddri=disabled  

	cd build
	ninja
	ninja install
}
