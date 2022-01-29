#!/bin/sh

docker pull computerfido/lemontoolchain:latest

SPATH=$(dirname $(readlink -f "$0"))

cd $SPATH/..

mkdir -p Build

cp Scripts/docker-bootstrap-site.yml Build/bootstrap-site.yml
echo "    create_extra_args: ['--user', '$(id -u):$(id -g)']" >> Build/bootstrap-site.yml

cd Build/
xbstrap init ..
docker run -v "$SPATH/..:/var/LemonOS" -w $(pwd) --user $(id -u):$(id -g) -it computerfido/lemontoolchain:latest sh -c "zstd -d /var/lemon-tools.tar.zst --stdout | tar -oxvf - -C /var/LemonOS --strip-components=1; chown -hR $(id -u) /var/LemonOS"
xbstrap install-tool --all