SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH
source ./env.sh

mkdir -p $HOME/.local/share/lemon/sysroot/system
mkdir -p $HOME/.local/share/lemon/sysroot/system/include
mkdir -p $HOME/.local/share/lemon/sysroot/system/lib
mkdir -p $HOME/.local/share/lemon/sysroot/system/bin

docker pull computerfido/lemontoolchain:latest
./docker-run.sh "$LEMON_BUILDROOT/Scripts/configure.sh"