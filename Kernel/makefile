.PHONY: all clean

ARCH := x86_64

SRCDIRS := src/arch/$(ARCH) src src/liballoc src/fs src/tty src/lai/core src/lai/helpers src/lai/drivers
INCLUDEPATH := -I include -I include/arch/$(ARCH)

CFLAGS-x86_64 := -Wno-write-strings -DLemon64 -ffreestanding -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -z max-page-size=0x1000 -nostdlib $(INCLUDEPATH)
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
CPP=x86_64-elf-g++
CC=x86_64-elf-gcc
endif


CFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.c")
CPPFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.cpp")
ASMFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.asm")
OBJFILES := $(patsubst %.cpp,%.cpp.o,$(CPPFILES)) $(patsubst %.asm,%.asm.o,$(ASMFILES)) $(patsubst %.c,%.c.o,$(CFILES))

.PHONY: all copy initrd

all: $(ARCH)

x86_64: $(OBJFILES)
	@x86_64-elf-g++ -m64 -T linkscript-x86_64.ld -o bin/$(ARCH)/kernel.sys $(CFLAGS) obj/$(ARCH)/*.asm.o obj/$(ARCH)/*.c.o obj/$(ARCH)/*.cpp.o -lgcc

x86: $(OBJFILES)
	@i686-lemon-g++ -T linkscript-x86.ld -o bin/$(ARCH)/kernel.sys $(CFLAGS) obj/$(ARCH)/*.asm.o obj/$(ARCH)/*.c.o obj/$(ARCH)/*.cpp.o -lgcc

initrd:
	@cd ../Applications/Init && ./build.sh && cp init.lef ../../InitrdWriter/Contents
	@cd ../Applications/FileManager && ./build.sh && cp fm.lef ../../InitrdWriter/Contents
	@cd ../Applications/Snake && ./build.sh && cp snake.lef ../../InitrdWriter/Contents
	@cd ../Applications/Shell && ./build.sh && cp shell.lef ../../InitrdWriter/Contents
	@cd ../Applications/TextEditor && ./build.sh && cp texteditor.lef ../../InitrdWriter/Contents
	@cd ../Applications/TerminalEmulator && ./build.sh && cp terminal.lef ../../InitrdWriter/Contents
	@cd ../Applications/SystemInfo && ./build.sh && cp sysinfo.lef ../../InitrdWriter/Contents
	@cd ../Applications/CHIP8Emu && ./build.sh && cp c8emu.lef ../../InitrdWriter/Contents
	@cd ../Applications/Draw && ./build.sh && cp draw.lef ../../InitrdWriter/Contents
	@cd ../Applications/Taskbar && ./build.sh && cp taskbar.lef ../../InitrdWriter/Contents
	@cd ../Applications/Solitaire && ./build.sh && cp solitaire.lef ../../InitrdWriter/Contents
	@cd ../Applications/ImageViewer && ./build.sh && cp imgview.lef ../../InitrdWriter/Contents
	@cd ../Applications/GUITest && ./build.sh && cp guitest.lef ../../InitrdWriter/Contents
	@make -C ../Applications/Gnuboy lemongnuboy && cp ../Applications/Gnuboy/lemongnuboy ../InitrdWriter/Contents
	
	@cd ../InitrdWriter && rm initrd.img && ./copy.sh
	@cp ../InitrdWriter/initrd.img ../initrd.img

copy:
	@cd .. && sudo ./copytodisk.sh

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
