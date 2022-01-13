#!/bin/sh
SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..
export LEMONDIR=$(pwd)

cd LibLemon
rm -r build
meson setup build --cross $SPATH/lemon-crossfile.txt

cd ../LibC
rm -r build
meson setup build --cross $SPATH/lemon-crossfile.txt

cd ../Applications
rm -r build
meson setup build --cross $SPATH/lemon-crossfile.txt

cd ../System
rm -r build
meson setup build --cross $SPATH/lemon-crossfile.txt

cd ../Kernel
rm -r build
meson setup build --cross $SPATH/lemon-crossfile.txt