SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..
export LEMONDIR=$(pwd)
cd $SPATH

export BINUTILS_SRC_DIR=binutils-2.32
export LLVM_SRC_DIR=llvm-project
 	
_unpack_binutils(){
    curl -L "http://ftpmirror.gnu.org/binutils/binutils-2.32.tar.gz" -o binutils-2.32.tar.gz
    tar -xzvf binutils-2.32.tar.gz
 	rm binutils-2.32.tar.gz
}

_unpack_llvm(){
    git clone "https://github.com/fido2020/llvm-project.git" $LLVM_SRC_DIR --depth 1
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

_build_llvm(){
    cd $LLVM_SRC_DIR

    mkdir build
    cd build
    cmake -C ../clang/cmake/caches/Lemon.cmake -DCMAKE_INSTALL_PREFIX=$TOOLCHAIN_PREFIX -DDEFAULT_SYSROOT=$LEMON_SYSROOT ../llvm -G Ninja

    ninja -j $JOBCOUNT
    ninja install
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
	
	curl https://api.lemonos.org/sysroot.tar.gz | tar -zxf - sysroot/system -C $LEMON_SYSROOT/..
}

_llvm(){
    _unpack_llvm
    _build_llvm
}
_clean(){
	rm -rf $LLVM_SRC_DIR $BINUTILS_SRC_DIR *.tar.*
}

_build(){
	_prepare
	cd $SPATH
	_binutils
	cd $SPATH
	_llvm
	cd $SPATH
	
	echo "Binutils and LLVM have been built."
}

if [ -z "$JOBCOUNT" ]; then
    export JOBCOUNT=$(nproc)
    echo "Compiling llvm will use a lot of memory, if you run out of memory try setting \$JOBCOUNT to a lower value (automatically set to $JOBCOUNT)."
    read
fi

if [ -z "$LEMON_SYSROOT" -o -z "$TOOLCHAIN_PREFIX" ]; then
    export TOOLCHAIN_PREFIX=$HOME/.local/share/lemon
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot
    echo "LEMON_SYSROOT or TOOLCHAIN_PREFIX not set, continuing will use defaults:\nTOOLCHAIN_PREFIX: $TOOLCHAIN_PREFIX\nLEMON_SYSROOT: $LEMON_SYSROOT"
    read
fi

if [ -z "$1" ]; then
    echo "Usage: $0 (clean/prepare/binutils/llvm/install_llvm/build)"
else
	cd $SPATH
    _$1
fi
