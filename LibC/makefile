
all:
	meson build --cross lemon-crossfile.txt
	ninja -C build
	cp build/libc.a bin/libc.a
	ninja -C build install
	cp bin/libc.a ../FakeSysroot/usr/lib/libc.a
	cp build/sysdeps/lemon/crt0.o ../FakeSysroot/usr/lib/crt0.o

clean:
	rm -rf build
