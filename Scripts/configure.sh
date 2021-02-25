SPATH=$(dirname $(readlink -f "$0"))

if [ -z "$LEMON_SYSROOT" ]; then
    export LEMON_SYSROOT=$HOME/.local/share/lemon/sysroot
fi

export PATH="$HOME/.local/share/lemon/bin:$PATH"

set -e

ln -sfT ../../../include/c++ $HOME/.local/share/lemon/sysroot/system/include/c++
cp $HOME/.local/share/lemon/lib/x86_64-lemon/c++/*.so* $HOME/.local/share/lemon/sysroot/system/lib

cd $SPATH
$SPATH/libc.sh

cd $SPATH/..
export LEMONDIR=$(pwd)

if ! [ -x "$(command -v lemon-clang)" ]; then
    echo "Lemon cross toolchain not found (Did you forget to build toolchain?)"
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

cd ../Ports
./buildport.sh zlib
./buildport.sh libpng
./buildport.sh freetype
./buildport.sh libressl
