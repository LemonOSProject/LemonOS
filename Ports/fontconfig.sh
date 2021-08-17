unpack(){
    if ! [ -f cache/fontconfig-2.13.94.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/fontconfig-2.13.94.tar.gz "https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.13.94.tar.gz"
    fi
    tar -xzvf cache/fontconfig-2.13.94.tar.gz
    export BUILD_DIR=fontconfig-2.13.94
}

buildp(){
 	cd $BUILD_DIR

    meson --cross "$LEMON_MESON_CROSSFILE" build
    ninja -C build install
}
