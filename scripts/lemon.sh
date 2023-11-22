#!/bin/bash

set -e

SPATH=$(dirname $(readlink -f "$0"))
export LEMON_BUILDROOT="$SPATH/.."
export LEMON_SYSROOT="$LEMON_BUILDROOT/build/sysroot"

export LIMINE_BIN_DIR="$LEMON_BUILDROOT/toolchain/limine/bin"

export MAKE_FLAGS=-j$(nproc)

if [ "$1" == "cmake" ]; then
    mkdir -p "$LEMON_BUILDROOT/build"
    cd "$LEMON_BUILDROOT/build"
    cmake .. -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCLANG_ASM_COMPILER=clang
    exit 0
fi

if [ "$1" == "toolchain" ]; then
    mkdir -p "$LEMON_BUILDROOT/toolchain"

    if [ ! -x "$LIMINE_BIN_DIR/limine" ]; then
        echo "building limine"
        "$LEMON_BUILDROOT/scripts/limine.sh"
    fi

    exit 0
fi

if [ "$1" == "iso" ]; then
    $LEMON_BUILDROOT/scripts/buildiso.sh
    exit 0
fi

echo "$0 <toolchain>"
