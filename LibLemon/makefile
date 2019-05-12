.PHONY: all clean

ARCH := x86

MAKEFILE_DIR := $(shell pwd)
SRCDIRS := src/gfx src/gfx/window
INCLUDEPATH := -I include -I ../LibC/include

CFLAGS-x86 := -fPIC -Wall -Wno-int-conversion -Wno-pointer-arith -Wno-write-strings -fno-exceptions -Wno-address -nostdlib -m32 -ffreestanding -Wno-write-strings -fno-exceptions -fno-rtti -std=c++14 $(INCLUDEPATH)
ASFLAGS-x86 := -f elf32 -g -F dwarf

CFILES := $(shell find $(SRCDIRS) -type f -name "*.cpp")
OBJFILES := $(patsubst %.asm,%.asm.o,$(ASMFILES)) $(patsubst %.cpp,%.cpp.o,$(CFILES))

ifeq ($(ARCH),x86)
CFLAGS=$(CFLAGS-x86)
endif

ifeq ($(ARCH),x86_64)
CFLAGS=$(CFLAGS-x86_64)
endif

.PHONY: all copy initrd

all: $(ARCH)

x86: $(OBJFILES)
	@ar rcs bin/liblemon.a obj/*

%.cpp.o: %.cpp
	@echo Compiling $<
	@i686-elf-gcc $(CFLAGS) -c $< -o obj/$(shell basename $@);

%.asm.o: %.asm
	@echo Assembling $<
	@nasm $(ASFLAGS) $< -o obj/$(shell basename $@); 
