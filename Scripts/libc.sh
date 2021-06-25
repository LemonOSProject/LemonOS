SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..
LEMONDIR=$(pwd)

if ! [ -x "$(command -v lemon-clang)" ]; then
	echo "Lemon cross toolchain not found (Did you forget to build toolchain? Or is it just not in PATH?)"
	exit 1
fi

cd $LEMONDIR/LibC
meson build --cross $SPATH/lemon-crossfile.txt
ninja -C build install

cd $LEMONDIR