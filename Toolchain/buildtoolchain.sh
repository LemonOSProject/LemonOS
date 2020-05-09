_unpack_binutils(){
    wget "ftpmirror.gnu.org/binutils/binutils-2.32.tar.gz"
    tar -xzvf binutils-2.32.tar.gz
 	export BINUTILS_BUILD_DIR=binutils-2.32
 	rm binutils-2.32.tar.gz
}

_unpack_gcc(){
    wget "http://ftpmirror.gnu.org/gcc/gcc-8.2.0/gcc-8.2.0.tar.gz"
    tar -xzvf gcc-8.2.0.tar.gz
 	export GCC_BUILD_DIR=gcc-8.2.0
 	rm gcc-8.2.0.tar.gz
}

_build_binutils(){
    cd $BINUTILS_BUILD_DIR
    patch -p1 < ../lemon-binutils-2.32.patch
    ./configure --target=x86_64-lemon --prefix=$TOOLCHAIN_PREFIX --with-sysroot=$LEMON_SYSROOT --disable-werror --enable-shared
    make -j$JOBCOUNT
    make install
}

_build_gcc(){
    cd $GCC_BUILD_DIR
    patch -p1 < ../lemon-gcc-8.2.0.patch
    ./configure --target=x86_64-lemon --prefix=$TOOLCHAIN_PREFIX --with-sysroot=$LEMON_SYSROOT --enable-languages=c,c++ --enable-shared
    make all-gcc all-target-libgcc -j$JOBCOUNT
    make install-gcc install-target-libgcc
}

_build_libstdcpp(){
    cd $GCC_BUILD_DIR
    make all-target-libstdc++-v3 -j$JOBCOUNT
    make install-target-libstdc++-v3
}

_binutils(){
    _unpack_binutils
    _build_binutils
}

_gcc(){
    _unpack_gcc
    _build_gcc
}

_libstdcpp(){
    _build_libstdcpp
}

if [ -z "$LEMON_SYSROOT" -o -z "$TOOLCHAIN_PREFIX" ]; then
    export TOOLCHAIN_PREFIX=$HOME/.local/share/lemon
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot
    echo "LEMON_SYSROOT or TOOLCHAIN_PREFIX not set, continuing will use defaults:\nTOOLCHAIN_PREFIX: $TOOLCHAIN_PREFIX\nLEMON_SYSROOT: $LEMON_SYSROOT"
    read
fi

if [ -z "$1" ]; then
    echo "Usage: $0 (binutils/gcc/libstdcpp)"
else
    _$1
fi