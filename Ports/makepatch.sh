#!/bin/sh

diff -ruN "$1" "$2" > "lemon-$1.patch"
