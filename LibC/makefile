.PHONY: all clean

ARCH := x86

SRCDIRS := src src/liballoc src/lemon src/string src/stdio src/ctype src/stdlib src/unistd src/sys src/math
INCLUDEPATH := -I include

CFLAGS-x86 := -O3 -fPIC -Wall -Wno-int-conversion -Wno-pointer-arith -fno-exceptions -Wno-address -nostdlib -m32 -ffreestanding -Wno-write-strings $(INCLUDEPATH)
ASFLAGS-x86 := -f elf32 -g -F dwarf

CFILES := $(shell find $(SRCDIRS) -type f -name "*.c")
ASMFILES := $(shell find $(SRCDIRS) -type f -name "*.S")
OBJFILES := $(patsubst %.S,%.o,$(ASMFILES)) $(patsubst %.c,%.c.o,$(CFILES))

ifeq ($(ARCH),x86)
CFLAGS=$(CFLAGS-x86)
endif

ifeq ($(ARCH),x86_64)
CFLAGS=$(CFLAGS-x86_64)
endif

.PHONY: all copy initrd

all: $(ARCH)

x86: $(OBJFILES)
	@echo $(OBJFILES)
	@ar rcs bin/libc.a obj/*
	@cp obj/crt0.o bin/crt0.o
	@cp bin/crt0.o ../FakeSysroot/usr/lib/crt0.o
	@cp obj/crt0.o bin/crti.o
	@cp bin/crt0.o ../FakeSysroot/usr/lib/crti.o
	@cp obj/crt0.o bin/crtn.o
	@cp bin/crt0.o ../FakeSysroot/usr/lib/crtn.o
	@rm -rf ../FakeSysroot/usr/include
	@cp -r include ../FakeSysroot/usr/

%.c.o: %.c
	@echo Compiling $<
	@i686-lemon-gcc $(CFLAGS) -c $< -o obj/$(shell basename $@);

%.o: %.S
	@echo Assembling $<
	@i686-lemon-gcc $(CFLAGS) -c $< -o obj/$(shell basename $@);

%.asm.o: %.asm
	@echo Assembling $<
	@nasm $(ASFLAGS) $< -o obj/$(shell basename $@); 
