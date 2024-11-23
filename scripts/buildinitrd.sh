#!/bin/sh

if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

cd "$LEMOND"

set -e

INITRDDIR="$LEMOND/build/initrd"

mkdir -p $INITRDDIR
cp $LEMOND/build/system/system $INITRDDIR

cd $INITRDDIR
tar -cf $LEMOND/build/initrd.tar *
cd $LEMOND
