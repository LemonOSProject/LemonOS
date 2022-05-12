#!/bin/sh

export LEMON_PREFIX="/system"
SPATH=$(dirname $(readlink -f "$0"))
export LEMON_BUILDROOT="$SPATH/.."
export LEMON_SYSROOT="$LEMON_BUILDROOT/Build/sysroot"
export LEMON_MESON_CROSSFILE="$LEMON_BUILDROOT/Scripts/lemon-crossfile.txt"
