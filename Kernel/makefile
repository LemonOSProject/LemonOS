.PHONY: all clean

ARCH := x86

SRCDIRS := src/arch/$(ARCH) src src/liballoc src/fs
INCLUDEPATH := -I include -I include/arch/$(ARCH)

#CFLAGS-x86_64 := -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -z max-page-size=0x1000 -nostdlib $(INCLUDEPATH)
#ASFLAGS-x86_64 := -f elf64 -g -F dwarf -w+gnu-elf-extensions

CFLAGS-x86 := -Wall -Wno-pointer-arith -fno-permissive -fno-exceptions -fno-rtti -std=c++14 -fno-pic -Wno-address -m32 -ffreestanding -Wno-write-strings -fno-stack-protector -nostdlib $(INCLUDEPATH)
ASFLAGS-x86 := -f elf32 -g -F dwarf

ifeq ($(ARCH),x86)
CFLAGS=$(CFLAGS-x86)
ASFLAGS=$(ASFLAGS-x86)
endif

ifeq ($(ARCH),x86_64)
CFLAGS=$(CFLAGS-x86_64)
ASFLAGS=$(ASFLAGS-x86_64)
endif


CFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.c")
CPPFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.cpp")
ASMFILES := $(shell find $(SRCDIRS) -maxdepth 1 -type f -name "*.asm")
OBJFILES := $(patsubst %.cpp,%.cpp.o,$(CPPFILES)) $(patsubst %.asm,%.asm.o,$(ASMFILES)) $(patsubst %.c,%.c.o,$(CFILES))

.PHONY: all copy initrd

all: $(ARCH)

x86_64: $(OBJFILES)
	@x86_64-elf-g++ -T linkscript-x86_64.ld -o bin/$(ARCH)/kernel.sys $(CFLAGS) obj/$(ARCH)/*.asm.o obj/$(ARCH)/*.c.o obj/$(ARCH)/*.cpp.o -lgcc

x86: $(OBJFILES)
	@i686-elf-g++ -T linkscript-x86.ld -o bin/$(ARCH)/kernel.sys $(CFLAGS) obj/$(ARCH)/*.asm.o obj/$(ARCH)/*.c.o obj/$(ARCH)/*.cpp.o -lgcc

initrd:
	@cd ../Applications/Init && ./build.sh && cp init.lef ../../InitrdWriter/Contents
	@cd ../Applications/FileManager && ./build.sh && cp fm.lef ../../InitrdWriter/Contents
	@cd ../Applications/Snake && ./build.sh && cp snake.lef ../../InitrdWriter/Contents
	@cd ../Applications/Shell && ./build.sh && cp shell.lef ../../InitrdWriter/Contents
	
	@cd ../InitrdWriter && rm initrd.img && ./copy.sh
	@cp ../InitrdWriter/initrd.img ../initrd.img

copy:
	@cd .. && sudo ./copytodisk.sh

dump:
	@cd bin/$(ARCH)/ && objdump -d kernel.sys > dump.txt

%.cpp.o: %.cpp
	@echo Compiling $<
	@i686-elf-g++ $(CFLAGS) -c $< -o obj/$(ARCH)/$(shell basename $@);

%.c.o: %.c
	@echo Compiling $<
	@i686-elf-gcc $(CFLAGS) -c $< -o obj/$(ARCH)/$(shell basename $@);

%.asm.o: %.asm
	@echo Assembling $<
	@nasm $(ASFLAGS) $< -o obj/$(ARCH)/$(shell basename $@);
