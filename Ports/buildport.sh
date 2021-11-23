if [ -z "$JOBCOUNT" ]; then
    export JOBCOUNT=$(nproc)
fi

export LEMON_BUILDROOT=$(readlink -f -- $(dirname $(readlink -f -- "$0"))/..)
. $LEMON_BUILDROOT/Scripts/env.sh

export PKG_CONFIG="$LEMON_BUILDROOT/Scripts/pkg-config"
export PATH="$PATH:$HOME/.local/share/lemon/bin"

export LD=x86_64-lemon-ld
export CC=lemon-clang
export CXX=lemon-clang++
export LEMON_PREFIX=/system

WORKING_DIR=$(pwd)
cd $LEMON_BUILDROOT/Ports
mkdir -p cache

. ./$1.sh

if [ "$2" != build ]; then
    unpack
fi

if [ "$2" != unpack ]; then
    buildp
fi

cd $WORKING_DIR
