unpack(){
 	git clone https://github.com/LemonOSProject/llvm-project.git --depth 1 llvm-project
 	export BUILD_DIR=llvm-project
}
 
buildp(){
 	cd $BUILD_DIR
  TBLGEN_DIR=$(pwd)/hostbuild/bin

  mkdir -p hostbuild
  cd hostbuild
  CC=gcc CXX=g++ cmake ../llvm -DLLVM_ENABLE_PROJECTS="clang" -G Ninja
  ninja llvm-tblgen clang-tblgen
  cd ..

 	mkdir -p build
  cd build
  cmake -DCMAKE_INSTALL_PREFIX=$LEMON_PREFIX -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_CROSSCOMPILING=True -DCMAKE_C_COMPILER=lemon-clang -DCMAKE_CXX_COMPILER=lemon-clang++ -DLLVM_ENABLE_LIBXML2=False ../llvm -G Ninja -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld -DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=lld -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-lemon -DLLVM_INSTALL_TOOLCHAIN_ONLY=true -DLLVM_TABLEGEN=$TBLGEN_DIR/llvm-tblgen -DCLANG_TABLEGEN=$TBLGEN_DIR/clang-tblgen -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_SYSROOT=$LEMON_SYSROOT -DCLANG_ENABLE_STATIC_ANALYZER=OFF -DCLANG_ENABLE_ARCMT=OFF -DCLANG_ENABLE_OBJC_REWRITER=OFF -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_THREADS=OFF -DLLVM_ENABLE_UNWIND_TABLES=OFF
  ninja -j$JOBCOUNT
  DESTDIR=$LEMON_SYSROOT ninja install
}
 