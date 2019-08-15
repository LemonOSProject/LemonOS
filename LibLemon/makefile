.PHONY: all clean

ARCH := x86

SRCDIRS := src src/gfx src/gfx/window
INCLUDEPATH := -I include

CFLAGS-x86 := -O3 -fPIC -Wall -Wno-int-conversion -Wno-pointer-arith -Wno-write-strings -fno-exceptions -Wno-address -nostdlib -m32 -ffreestanding -Wno-write-strings -fno-exceptions -fno-rtti -std=c++14 $(INCLUDEPATH)
ASFLAGS-x86 := -f elf32 -g -F dwarf

CFILES := $(shell find $(SRCDIRS) -type f -name "*.cpp")
ASMFILES := $(shell find $(SRCDIRS) -type f -name "*.asm")
OBJFILES := $(patsubst %.asm,%.asm.o,$(ASMFILES)) $(patsubst %.cpp,%.cpp.o,$(CFILES))

ifeq ($(ARCH),x86)
CFLAGS=$(CFLAGS-x86)
ASFLAGS=$(ASFLAGS-x86)
endif

ifeq ($(ARCH),x86_64)
CFLAGS=$(CFLAGS-x86_64)
endif

.PHONY: all copy initrd

all: $(ARCH)

x86: $(OBJFILES)
	@ar rcs bin/liblemon.a obj/*
	#@cp bin/liblemon.a ../FakeSysroot/usr/lib/
	@cp -r include ../FakeSysroot/usr/

%.cpp.o: %.cpp
	@echo Compiling $<
	@i686-lemon-gcc $(CFLAGS) -c $< -o obj/$(shell basename $@);

%.asm.o: %.asm
	@echo Assembling $<
	@nasm $(ASFLAGS) $< -o obj/$(shell basename $@); 
