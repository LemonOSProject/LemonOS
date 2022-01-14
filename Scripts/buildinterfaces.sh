#!/bin/sh

set -e

SPATH=$(dirname $(readlink -f "$0"))
source $SPATH/env.sh

INCLUDEDIR="$LEMON_SYSROOT/system/include/Lemon/Services/"

LIC="$LEMON_BUILDROOT/InterfaceCompiler/lic"
LIC_SRC="$LEMON_BUILDROOT/InterfaceCompiler/main.cpp"

WD="$(pwd)"

if ! [ -f "$LIC" ] || [ "$LIC_SRC" -nt "$LIC" ]; then # If the lic executable doesent exist or is older than source, build it
    echo "Rebuilding $LIC_SRC"
    cd "$LEMON_BUILDROOT/InterfaceCompiler"
    ./build.sh
    cd "$WD"
fi


mkdir -p "$INCLUDEDIR" 
cd "$LEMON_BUILDROOT/Services"

"$LIC" lemon.lemond.li "$INCLUDEDIR/lemon.lemond.h"
"$LIC" lemon.networkgovernor.li "$INCLUDEDIR/lemon.networkgovernor.h"
"$LIC" lemon.lemonwm.li "$INCLUDEDIR/lemon.lemonwm.h"
"$LIC" lemon.shell.li "$INCLUDEDIR/lemon.shell.h"
