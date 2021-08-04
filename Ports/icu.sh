unpack(){
    if ! [ -f cache/icu4c-69_1-src.tgz ]; then # Check if tarball exists
 		curl -Lo cache/icu4c-69_1-src.tgz "https://github.com/unicode-org/icu/releases/download/release-69-1/icu4c-69_1-src.tgz"
	fi

 	tar -xzvf cache/icu4c-69_1-src.tgz
 	export BUILD_DIR=icu
}

buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-icu-69.1.patch

 	mkdir -p hostbuild
 	mkdir -p lemonbuild

 	cd hostbuild # We need a host build of ICU
    CC=gcc CXX=g++ ../source/configure --prefix=/usr
    make -j$JOBCOUNT

 	cd ../lemonbuild
    ../source/configure --prefix=$LEMON_PREFIX --host=x86_64-lemon --sbindir=/system/bin --sysconfdir=/system/share --with-cross-build=$(readlink -f "$(pwd)/../hostbuild")

 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
