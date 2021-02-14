export JOBCOUNT=$(nproc)

if [ -z "$LEMON_SYSROOT" ]; then
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot

    echo "warning: LEMON_SYSROOT not set. Automatically set to $LEMON_SYSROOT"
    read
fi

export CC=lemon-clang
export CXX=lemon-clang++
export CFLAGS=-Wno-error
export LEMON_PREFIX=/system

. ./$1.sh

if [ "$2" != build ]; then
    unpack 
fi

if [ "$2" != unpack ]; then
    buildp
fi
