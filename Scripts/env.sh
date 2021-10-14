#!/bin/sh

export LEMON_SYSROOT="$HOME/.local/share/lemon/sysroot"
export LEMON_MESON_CROSSFILE="$LEMON_BUILDROOT/Scripts/lemon-crossfile.txt"
export LEMON_PREFIX="/system"
SPATH=$(dirname $(readlink -f "$0"))
export LEMON_BUILDROOT="$SPATH/.."
export PATH="$PATH:$HOME/.local/share/lemon/bin"
