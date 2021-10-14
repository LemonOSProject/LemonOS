unpack(){
    if ! [ -f cache/cache/curl-7.77.0.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/curl-7.77.0.tar.gz "https://curl.se/download/curl-7.77.0.tar.gz"
    fi
    tar -xzf cache/curl-7.77.0.tar.gz
    export BUILD_DIR=curl-7.77.0
}

buildp(){
 	cd $BUILD_DIR
    unset PKG_CONFIG # curl fails when pkg-config is set

 	patch -p1 < ../lemon-curl-7.77.0.patch
 	# TODO: Network stack isnt the most stable
 	curl_disallow_alarm=yes curl_disallow_getpeername=yes curl_disallow_getsockname=yes ./configure --host=x86_64-lemon --prefix=/system --with-openssl --disable-ipv6 --disable-unix-sockets --disable-pthreads --disable-threaded-resolver

 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
