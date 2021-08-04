#!/bin/sh

export LEMON_SYSROOT="$HOME/.local/share/lemon/sysroot"
export LEMON_PREFIX="/system"
SPATH=$(dirname $(readlink -f "$0"))
export LEMON_BUILDROOT="$SPATH/.."