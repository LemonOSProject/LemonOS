FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \ 
    build-essential autoconf libtool python3 python3-pip ninja-build qemu-utils nasm help2man gettext autopoint gperf git cmake curl texinfo wget flex

RUN python3 -m ensurepip; python3 -m pip install meson xbstrap

WORKDIR /var
RUN set -e; git clone https://github.com/LemonOSProject/LemonOS --depth 1; find LemonOS/ -maxdepth 1 -mindepth 1 -type d \
    -not -name patches -not -name Ports -not -name Scripts -exec rm -rf {} +
RUN cd LemonOS; mkdir Build; cd Build; xbstrap init ..
RUN cd LemonOS/Build; xbstrap compile-tool --all;
RUN apt-get install zstd; mkdir -p /var/lemon-tools/Build; mv /var/LemonOS/Toolchain /var/lemon-tools; mv /var/LemonOS/Build/tool-builds /var/lemon-tools/Build/tool-builds; \
    tar -cvf - lemon-tools | zstd - -o lemon-tools.tar.zst; rm -rf LemonOS lemon-tools; chmod 644 /var/lemon-tools.tar.zst

FROM ubuntu:latest
RUN apt-get update && apt-get install -y \ 
    build-essential autoconf libtool python3 python3-pip ninja-build qemu-utils nasm help2man gettext autopoint gperf git cmake curl texinfo wget flex unzip rsync \
    e2fsprogs dosfstools zstd; \
    python3 -m ensurepip; python3 -m pip install meson xbstrap

WORKDIR /var
COPY --from=0 /var/lemon-tools.tar.zst ./
