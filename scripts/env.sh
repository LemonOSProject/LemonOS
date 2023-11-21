#!/bin/bash

SPATH=$(dirname $(readlink -f "$0"))
export LEMON_BUILDROOT="$SPATH/.."
export LEMON_SYSROOT="$LEMON_BUILDROOT/build/sysroot"
