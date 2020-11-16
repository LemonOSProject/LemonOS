export JOBCOUNT=$(nproc)

if [ -z "$LEMON_SYSROOT" ]; then
    echo "error: LEMON_SYSROOT not set"
    exit
fi

export CC=lemon-clang
export CFLAGS=-Wno-error
export LEMON_PREFIX=/system

. ./$1.sh

unpack 
buildp
