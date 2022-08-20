#!/bin/sh

set -e

INCLUDEDIR="$2/system/include/lemon/protocols/"

LIC="$1/scripts/lic/lic"
LIC_SRC="$1/scripts/lic/main.cpp"

WD="$(pwd)"

if ! [ -f "$LIC" ] || [ "$LIC_SRC" -nt "$LIC" ]; then # If the lic executable doesent exist or is older than source, build it
    echo "Rebuilding $LIC_SRC"
    cd "$1/scripts/lic"
    ./build.sh
    cd "$WD"
fi

mkdir -p "$INCLUDEDIR" 
cd "$1/protocols"

"$LIC" lemon.lemond.li "$INCLUDEDIR/lemon.lemond.h"
"$LIC" lemon.networkgovernor.li "$INCLUDEDIR/lemon.networkgovernor.h"
"$LIC" lemon.lemonwm.li "$INCLUDEDIR/lemon.lemonwm.h"
"$LIC" lemon.shell.li "$INCLUDEDIR/lemon.shell.h"
