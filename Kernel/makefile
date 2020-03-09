.PHONY: all clean

ARCH := x86_64

SRCDIRS := src/arch/$(ARCH) src src/liballoc src/fs src/tty src/lai/core src/lai/helpers src/lai/drivers
INCLUDEPATH := -I include -I include/arch/$(ARCH)

CFLAGS-x86_64 := -Wno-write-strings -Wno-pointer-arith -DLemon64 -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -z max-page-size=0x1000 -nostdlib -fno-pic -fno-stack-protector -fno-exceptions -fno-rtti -fno-stack-protector $(INCLUDEPATH)
ASFLAGS-x86_64 := -f elf64 -g -F dwarf -w+gnu-elf-extensions

CFLAGS-x86 := -DLemon32 -fno-pic -Wall -Wno-pointer-arith -fno-permissive -fno-exceptions -fno-rtti -std=c++14 -fno-pic -Wno-address -m32 -ffreestanding -Wno-write-strings -fno-stack-protector -nostdlib $(INCLUDEPATH)
ASFLAGS-x86 := -f elf32 -g -F dwarf

ifeq ($(ARCH),x86)
CFLAGS=$(CFLAGS-x86)
ASFLAGS=$(ASFLAGS-x86)
CC=i686-lemon-gcc
CPP=i686-lemon-g++
endif

ifeq ($(ARCH),x86_64)
CFLAGS=$(CFLAGS-x86_64)
ASFLAGS=$(ASFLAGS-x86_64)
CPP=x86_64-lemon-g++
CC=x86_64-lemon-gcc
endif


CFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.c")
CPPFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.cpp")
ASMFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.asm")
OBJFILES := $(patsubst %.cpp,%.cpp.o,$(CPPFILES)) $(patsubst %.asm,%.asm.o,$(ASMFILES)) $(patsubst %.c,%.c.o,$(CFILES))

.PHONY: all copy initrd

all: $(ARCH)

x86_64: $(OBJFILES)
	@x86_64-lemon-g++ -m64 -T linkscript-x86_64.ld -o bin/$(ARCH)/kernel.sys $(CFLAGS) obj/$(ARCH)/*.asm.o obj/$(ARCH)/*.c.o obj/$(ARCH)/*.cpp.o obj/$(ARCH)/initrd.o -lgcc

x86: $(OBJFILES)
	@i686-lemon-g++ -T linkscript-x86.ld -o bin/$(ARCH)/kernel.sys $(CFLAGS) obj/$(ARCH)/*.asm.o obj/$(ARCH)/*.c.o obj/$(ARCH)/*.cpp.o -lgcc

initrd:
	@cd ../Applications/Init && ./build.sh && cp init.lef ../../InitrdWriter/Contents
	@cd ../Applications/FileManager && ./build.sh && cp fileman.lef ../../InitrdWriter/Contents
	@cd ../Applications/Snake && ./build.sh && cp snake.lef ../../InitrdWriter/Contents
	@cd ../Applications/Shell && ./build.sh && cp shell.lef ../../InitrdWriter/Contents
	@cd ../Applications/TextEdit && ./build.sh && cp textedit.lef ../../InitrdWriter/Contents
	
	@cd ../InitrdWriter && ./copy.sh
	@cp ../InitrdWriter/initrd-$(ARCH).img ../initrd.img
	@cd .. && ./convertinitrd.sh

copy:
	@cd .. && sudo ./copytodisk.sh

copy_wsl:
	@cd .. && cmd.exe /c build-win\\copytodisk.bat

dump:
	@cd bin/$(ARCH)/ && objdump -d kernel.sys > dump.txt

clean:
	@rm obj/x86/*
	@rm obj/x86_64/*
	@rm ../Applications/*/*.lef
	@rm ../Applications/*/*.o

%.cpp.o: %.cpp
	@echo Compiling $<
	@$(CPP) $(CFLAGS) -c $< -o obj/$(ARCH)/$(shell basename $@);

%.c.o: %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o obj/$(ARCH)/$(shell basename $@);

%.asm.o: %.asm
	@echo Assembling $<
	@nasm $(ASFLAGS) $< -o obj/$(ARCH)/$(shell basename $@);
