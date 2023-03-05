#!/bin/sh

set -e 1

docker pull computerfido/lemontoolchain:latest

SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..

mkdir -p Build
ln -sfT Build build
ln -sfT Toolchain toolchain
ln -sfT ports Ports

cp scripts/docker-bootstrap-site.yml build/bootstrap-site.yml
echo "    create_extra_args: ['--user', '$(id -u):$(id -g)']" >> build/bootstrap-site.yml

cd build/
xbstrap init ..
docker run -v "$SPATH/..:/var/LemonOS" -w $(pwd) --user $(id -u):$(id -g) -it computerfido/lemontoolchain:latest sh -c "zstd -d /var/lemon-tools.tar.zst --stdout | tar -oxvf - -C /var/LemonOS --strip-components=1; chown -hR $(id -u) /var/LemonOS"

xbstrap regenerate icu gettext
xbstrap install-tool host-icu --reconfigure
xbstrap install-tool --all
