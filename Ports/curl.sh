unpack(){
    if ![ -f cache/cache/curl-7.77.0.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/curl-7.77.0.tar.gz "https://curl.se/download/curl-7.77.0.tar.gz"
    fi
    tar -xzvf cache/curl-7.77.0.tar.gz
    export BUILD_DIR=curl-7.77.0
}

buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-curl-7.77.0.patch
 	curl_disallow_getpeername=yes curl_disallow_getsockname=yes ./configure --host=x86_64-lemon --prefix=/system --with-openssl --disable-ipv6

 	make $JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
