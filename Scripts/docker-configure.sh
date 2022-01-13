#!/bin/sh
SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..
export LEMONDIR=$(pwd)

mkdir -p $HOME/.local/share/lemon/sysroot/system
mkdir -p $HOME/.local/share/lemon/sysroot/system/include
mkdir -p $HOME/.local/share/lemon/sysroot/system/lib
mkdir -p $HOME/.local/share/lemon/sysroot/system/bin

docker pull computerfido/lemontoolchain:latest
docker run -v $(pwd):$(pwd) -v "$HOME/.local/share/lemon/sysroot":"/root/.local/share/lemon/sysroot" -w $(pwd) --user $(id -u):$(id -g) -it computerfido/lemontoolchain:latest sh -c "HOME=/root LEMON_SYSROOT=/root/.local/share/lemon/sysroot $LEMONDIR/Scripts/configure.sh"