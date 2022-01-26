#!/bin/sh

set -e

INCLUDEDIR="$2/system/include/Lemon/Services/"

LIC="$1/InterfaceCompiler/lic"
LIC_SRC="$1/InterfaceCompiler/main.cpp"

WD="$(pwd)"

if ! [ -f "$LIC" ] || [ "$LIC_SRC" -nt "$LIC" ]; then # If the lic executable doesent exist or is older than source, build it
    echo "Rebuilding $LIC_SRC"
    cd "$1/InterfaceCompiler"
    ./build.sh
    cd "$WD"
fi

mkdir -p "$INCLUDEDIR" 
cd "$1/Services"

"$LIC" lemon.lemond.li "$INCLUDEDIR/lemon.lemond.h"
"$LIC" lemon.networkgovernor.li "$INCLUDEDIR/lemon.networkgovernor.h"
"$LIC" lemon.lemonwm.li "$INCLUDEDIR/lemon.lemonwm.h"
"$LIC" lemon.shell.li "$INCLUDEDIR/lemon.shell.h"
