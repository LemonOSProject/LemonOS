unpack(){
    if ! [ -f cache/Python-3.8.2.tgz ]; then # Check if tarball exists
        curl -Lo cache/Python-3.8.2.tgz "https://www.python.org/ftp/python/3.8.2/Python-3.8.2.tgz"
    fi

 	tar -xzvf cache/Python-3.8.2.tgz
 	export BUILD_DIR=Python-3.8.2
}

buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-Python-3.8.2.patch
 	
	autoreconf -f
	CC=gcc CXX=g++ ./configure
	
	CC=gcc CXX=g++ make python Parser/pgen -j$JOBCOUNT # You need python to build python so build host python
	mv python hostpython
	mv Parser/pgen Parser/hostpgen
	make distclean

	# For some reason setup.py gets run without _PYTHON_HOST_PLATFORM being set otherwise
    _PYTHON_HOST_PLATFORM=x86_64-lemon PYTHON_FOR_BUILD=./hostpython ac_cv_file__dev_ptmx=yes ac_cv_file__dev_ptc=no \
	./configure --host=x86_64-lemon --disable-ipv6 --build=x86_64 --prefix=/system --without-ensurepip --with-sysroot=$LEMON_SYSROOT --disable-shared
    
 	_PYTHON_HOST_PLATFORM=x86_64-lemon make -j$JOBCOUNT HOSTPYTHON=./hostpython HOSTPGEN=./Parser/hostpgen
 	_PYTHON_HOST_PLATFORM=x86_64-lemon make -j$JOBCOUNT install DESTDIR=$LEMON_SYSROOT
}
