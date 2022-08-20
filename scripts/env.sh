#!/bin/sh

export LEMON_PREFIX="/system"
SPATH=$(dirname $(readlink -f "$0"))
export LEMON_BUILDROOT="$SPATH/.."
export LEMON_SYSROOT="$LEMON_BUILDROOT/build/sysroot"
export LEMON_MESON_CROSSFILE="$LEMON_BUILDROOT/scripts/lemon-crossfile.txt"
