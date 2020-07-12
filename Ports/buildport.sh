export JOBCOUNT=$(nproc)

if [ -z "$LEMON_SYSROOT" ]; then
    echo "error: LEMON_SYSROOT not set"
    exit
fi

export LEMON_PREFIX=/system

. ./$1.sh

unpack 
buildp
