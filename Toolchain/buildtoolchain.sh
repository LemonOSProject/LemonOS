SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..
export LEMONDIR=$(pwd)
cd $SPATH

export JOBCOUNT=$(nproc)

export BINUTILS_SRC_DIR=binutils-2.32
export GCC_SRC_DIR=gcc-10.1.0
 	
_unpack_binutils(){
    wget "http://ftpmirror.gnu.org/binutils/binutils-2.32.tar.gz"
    tar -xzvf binutils-2.32.tar.gz
 	rm binutils-2.32.tar.gz
}

_unpack_gcc(){
    wget "http://ftpmirror.gnu.org/gcc/gcc-10.1.0/gcc-10.1.0.tar.gz"
    tar -xzvf gcc-10.1.0.tar.gz
 	rm gcc-10.1.0.tar.gz
}

_build_binutils(){
    cd $BINUTILS_SRC_DIR
    patch -p1 < ../lemon-binutils-2.32.patch
    cd ld
    aclocal
    automake
    autoreconf
    cd ..
    ./configure --target=x86_64-lemon --prefix=$TOOLCHAIN_PREFIX --with-sysroot=$LEMON_SYSROOT --disable-werror --enable-shared
    make -j $JOBCOUNT
    make install
}

_build_gcc(){
	mkdir build-gcc
    cd $GCC_SRC_DIR
    patch -p1 < ../lemon-gcc-10.1.0.patch
    cd ../build-gcc
    ../$GCC_SRC_DIR/configure --target=x86_64-lemon --prefix=$TOOLCHAIN_PREFIX --with-sysroot=$LEMON_SYSROOT --enable-languages=c,c++ --enable-shared
    make all-gcc all-target-libgcc -j $JOBCOUNT
    make install-gcc install-target-libgcc
    
    cp $TOOLCHAIN_PREFIX/x86_64-lemon/lib/*.so* $LEMON_SYSROOT/system/lib/
}

_build_libstdcpp(){
	cd gcc-*/libstdc++-v3/
	autoconf
	cd ../../
    cd build-gcc
    make all-target-libstdc++-v3 -j $JOBCOUNT
    make install-target-libstdc++-v3
}

_binutils(){
    _unpack_binutils
    _build_binutils
}

_prepare(){
	mkdir -p $LEMON_SYSROOT/system
	mkdir -p $LEMON_SYSROOT/system/include
	mkdir -p $LEMON_SYSROOT/system/lib
	mkdir -p $LEMON_SYSROOT/system/bin
	
	cd $LEMONDIR/LibC
	meson build --cross $LEMONDIR/Scripts/lemon-crossfile.txt -Dheaders_only=true
	meson install -Cbuild
	rm -rf build
	
	cd $SPATH
}

_gcc(){
    _unpack_gcc
    _build_gcc
}

_libstdcpp(){
    _build_libstdcpp
}

_clean(){
	rm -rf $GCC_SRC_DIR $BINUTILS_SRC_DIR *.tar.*
}

_build(){
	_prepare
	cd $SPATH
	_binutils
	cd $SPATH
	_gcc
	cd $SPATH
	
	echo "Binutils and GCC have been built, after the C library has been built run \"$0 libstdcpp\""
}

if [ -z "$LEMON_SYSROOT" -o -z "$TOOLCHAIN_PREFIX" ]; then
    export TOOLCHAIN_PREFIX=$HOME/.local/share/lemon
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot
    echo "LEMON_SYSROOT or TOOLCHAIN_PREFIX not set, continuing will use defaults:\nTOOLCHAIN_PREFIX: $TOOLCHAIN_PREFIX\nLEMON_SYSROOT: $LEMON_SYSROOT"
    read
fi

if [ -z "$1" ]; then
    echo "Usage: $0 (clean/prepare/binutils/gcc/libstdcpp/build)"
else
	cd $SPATH
    _$1
fi
