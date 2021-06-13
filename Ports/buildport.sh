export JOBCOUNT=$(nproc)

export LEMON_BUILDROOT=$(readlink -f -- $(dirname $(readlink -f -- "$0"))/..)

if [ -z "$LEMON_SYSROOT" ]; then
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot

    echo "warning: LEMON_SYSROOT not set. Automatically set to $LEMON_SYSROOT"
fi

export CC=lemon-clang
export CXX=lemon-clang++
export CFLAGS=-Wno-error
export LEMON_PREFIX=/system

WORKING_DIR=$(pwd)
cd $LEMON_BUILDROOT/Ports

. ./$1.sh

if [ "$2" != build ]; then
    unpack 
fi

if [ "$2" != unpack ]; then
    buildp
fi

cd $WORKING_DIR