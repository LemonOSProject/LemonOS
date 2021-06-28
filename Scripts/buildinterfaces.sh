#!/bin/sh

set -e

if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

if [ -z "$LEMON_SYSROOT" ]; then
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot
fi

INCLUDEDIR="$LEMON_SYSROOT/system/include/Lemon/Services/"

LIC="$LEMOND/InterfaceCompiler/lic"
LIC_SRC="$LEMOND/InterfaceCompiler/main.cpp"

WD="$(pwd)"

if ! [ -f "$LIC" ] || [ "$LIC_SRC" -nt "$LIC" ]; then # If the lic executable doesent exist or is older than source, build it
    cd "$LEMOND/InterfaceCompiler"
    ./build.sh
    cd "$WD"
fi


mkdir -p "$INCLUDEDIR" 
cd "$LEMOND/Services"

"$LIC" lemon.lemond.li "$INCLUDEDIR/lemon.lemond.h"
"$LIC" lemon.networkgovernor.li "$INCLUDEDIR/lemon.networkgovernor.h"
"$LIC" lemon.lemonwm.li "$INCLUDEDIR/lemon.lemonwm.h"