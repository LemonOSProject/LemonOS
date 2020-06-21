if [ -z "$LEMON_SYSROOT" ]; then
    echo "error: LEMON_SYSROOT not set"
    exit
fi

. ./$1.sh

unpack 
buildp
