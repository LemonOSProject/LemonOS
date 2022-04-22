FROM alpine:3.12

RUN apk add --no-cache nasm python3 python3-dev ninja make cmake automake autoconf libtool make gcc g++ musl-dev git curl patch zstd coreutils \ 
    # automake needs GNU gzip
    gzip \
    # libtool
    help2man xz \
    # wayland-scanner
    expat-dev \  
    # used by binutils
    texinfo bison flex \
    # limine, disk image
    mtools dosfstools e2fsprogs \
    # coreutils
    gperf gettext

RUN python3 -m ensurepip; python3 -m pip install meson xbstrap

WORKDIR /var
RUN set -e; git clone https://github.com/LemonOSProject/LemonOS --depth 1; find LemonOS/ -maxdepth 1 -mindepth 1 -type d \
    -not -name patches -not -name Ports -not -name Scripts -exec rm -rf {} +
RUN cd LemonOS; mkdir Build; cd Build; xbstrap init ..
RUN cd LemonOS/Build; xbstrap compile-tool --all;
RUN mkdir -p /var/lemon-tools/Build; mv /var/LemonOS/Toolchain /var/lemon-tools; mv /var/LemonOS/Build/tool-builds /var/lemon-tools/Build/tool-builds; \
    tar -cvf - lemon-tools | zstd - -o lemon-tools.tar.zst; rm -rf LemonOS lemon-tools; chmod 644 /var/lemon-tools.tar.zst
