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

mkdir -p "$INCLUDEDIR" 
cd "$LEMOND/Services"

"$LIC" lemon.lemond.li "$INCLUDEDIR/lemon.lemond.h"
"$LIC" lemon.networkgovernor.li "$INCLUDEDIR/lemon.networkgovernor.h"
"$LIC" lemon.lemonwm.li "$INCLUDEDIR/lemon.lemonwm.h"