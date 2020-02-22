.PHONY: all clean

ARCH := x86_64

SRCDIRS := src src/gfx
INCLUDEPATH := -I include

CFLAGS-x86 := -O3 -fPIC -Wall -Wno-int-conversion -Wno-pointer-arith -Wno-write-strings -fno-exceptions -Wno-address -nostdlib -m32 -ffreestanding -fno-rtti -std=c++14 $(INCLUDEPATH)
ASFLAGS-x86 := -f elf32 -g -F dwarf

CFLAGS-x86_64 := -fPIC -Wall -Wno-int-conversion -Wno-pointer-arith -Wno-write-strings -fno-exceptions -fno-rtti -Wno-address -m64 -std=c++14 $(INCLUDEPATH) -O3
ASFLAGS-x86_64 := -f elf64 -g -F dwarf

CFILES := $(shell find $(SRCDIRS) -type f -name "*.cpp")
ASMFILES := $(shell find $(SRCDIRS) -type f -name "*.asm")
OBJFILES := $(patsubst %.asm,%.asm.o,$(ASMFILES)) $(patsubst %.cpp,%.cpp.o,$(CFILES))

ifeq ($(ARCH),x86)
CFLAGS=$(CFLAGS-x86)
ASFLAGS=$(ASFLAGS-x86)
endif

ifeq ($(ARCH),x86_64)
CFLAGS=$(CFLAGS-x86_64)
ASFLAGS=$(ASFLAGS-x86_64)
endif

.PHONY: all copy initrd

all: $(ARCH)

x86: $(OBJFILES)
	@ar rcs bin/liblemon.a obj/*
	@cp -r include ~/.local/share/lemon/sysroot/usr/

x86_64: $(OBJFILES)
	@ar rcs bin/liblemon.a obj/*
	@cp bin/liblemon.a ~/.local/share/lemon/sysroot/usr/lib
	@cp -r include ~/.local/share/lemon/sysroot/usr/

%.c.o: %.c
	@echo Compiling $<
	@x86_64-lemon-gcc $(CFLAGS) -c $< -o obj/$(shell basename $@);

%.cpp.o: %.cpp
	@echo Compiling $<
	@x86_64-lemon-gcc $(CFLAGS) -c $< -o obj/$(shell basename $@);

%.asm.o: %.asm
	@echo Assembling $<
	@nasm $(ASFLAGS) $< -o obj/$(shell basename $@); 
