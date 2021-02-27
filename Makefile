JOBS := $(shell nproc)

.PHONY: all disk kernel base initrd libc liblemon system clean run vbox debug

all: kernel libc base initrd disk

libc:
	ninja -j$(JOBS) -C LibC/build install
	
liblemon:
	ninja -j$(JOBS) -C LibLemon/build install
	
applications: liblemon
	ninja -j$(JOBS) -C Applications/build install

system: liblemon
	ninja -j$(JOBS) -C System/build install

kernel:
	ninja -j$(JOBS) -C Kernel/build
	
userspace: liblemon applications
	
initrd: libc
	Scripts/buildinitrd.sh
	
base: applications system
	Scripts/buildbase.sh

disk:
	Scripts/build-nix/copytodisk.sh
	
clean:
	ninja -C LibC/build clean
	ninja -C LibLemon/build clean
	ninja -C Applications/build clean
	ninja -C Kernel/build clean
	ninja -C System/build clean
	rm -rf Initrd/*
	rm initrd.tar
	
cleanall:
	rm -rf LibC/build LibLemon/build Applications/build Kernel/build System/build
	rm -rf Initrd/*
	rm initrd.tar
	
run:
	Scripts/run.sh

qemu: run

qemuusb:
	Scripts/run.sh qemuusb

qemunetdump:
	Scripts/run.sh qemunetdump

vbox:
	Scripts/run.sh vbox
	
debug:
	Scripts/run.sh debug_qemu
