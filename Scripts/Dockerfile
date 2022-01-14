FROM alpine:3.12

RUN apk add --no-cache nasm python3 ninja make cmake automake autoconf libtool make gcc g++ musl-dev git curl patch e2fsprogs dosfstools

RUN cd /root && curl https://raw.githubusercontent.com/fido2020/Lemon-OS/master/Toolchain/buildtoolchain.sh -o buildtoolchain.sh && curl https://raw.githubusercontent.com/fido2020/Lemon-OS/master/Toolchain/lemon-binutils-2.32.patch -o lemon-binutils-2.32.patch && chmod 755 /root/buildtoolchain.sh

RUN /root/buildtoolchain.sh prepare
RUN /root/buildtoolchain.sh binutils
RUN /root/buildtoolchain.sh llvm CC=gcc CXX=g++
RUN /root/buildtoolchain.sh limine
RUN if [ ! -f "/root/.local/share/lemon/bin/clang" ]; then ninja -C /root/llvm-project/build install; fi && ln -sf /root/.local/share/lemon/bin/clang /root/.local/share/lemon/bin/lemon-clang && ln -sf /root/.local/share/lemon/bin/clang++ /root/.local/share/lemon/bin/lemon-clang++ && ln -sf ../../include/c++ /root/.local/share/lemon/sysroot/system/include/c++ \
    && cd /root/.local/share/lemon/bin && rm clang-repl clang-scan-deps clang-check clang-refactor clang-rename clang-extdef-mapping diagtool \
    && /root/buildtoolchain.sh clean && python3 -m ensurepip --default-pip && python3 -m pip install meson && chmod -R a+rx /root

ENV PATH=$PATH:/root/.local/share/lemon/bin:/root/.local/share/lemon/x86_64-lemon/bin/
