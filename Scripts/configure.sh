SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..
export LEMONDIR=$(pwd)

if ! [ -x "$(command -v x86_64-lemon-gcc)" ]; then
	echo "Lemon cross toolchain not found (Did you forget to build toolchain? Or is it just not in PATH?)"
	exit 1
fi

cd LibLemon
meson build --cross $SPATH/lemon-crossfile.txt

cd ../Applications
meson build --cross $SPATH/lemon-crossfile.txt

cd ../System
meson build --cross $SPATH/lemon-crossfile.txt

cd ../Kernel
meson build --cross $SPATH/lemon-crossfile.txt

if [ -z "$LEMON_SYSROOT" ]; then
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot
    echo "LEMON_SYSROOT not set, continuing will use defaults:\n$LEMON_SYSROOT\n"
    read
fi

cd ../Ports
./buildport.sh zlib
./buildport.sh libpng
./buildport.sh freetype
./buildport.sh lemon-binutils
./buildport.sh nyancat
