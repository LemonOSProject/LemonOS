SPATH=$(dirname $(readlink -f "$0"))

set -e

cd $SPATH/..
docker run -v $(pwd):$(pwd) -v "$HOME/.local/share/lemon/sysroot":"/root/.local/share/lemon/sysroot" -w $(pwd) --user $(id -u):$(id -g) -it computerfido/lemontoolchain:latest sh -c "HOME=/root $*"