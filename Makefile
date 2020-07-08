JOBS := $(shell nproc)

.PHONY: disk

libc:
	ninja -C LibC/build install -j $(JOBS)
	
liblemon:
	ninja -C LibLemon/build install -j $(JOBS)
	
applications: liblemon
	ninja -C Applications/build install -j $(JOBS)

kernel:
	ninja -C Kernel/build
	
userspace: liblemon applications
	
initrd: libc liblemon
	Scripts/buildinitrd.sh
	
base: applications
	Scripts/buildbase.sh

disk: kernel initrd base
	Scripts/build-nix/copytodisk.sh
	
run:
	Scripts/run.sh

qemu: run

vbox:
	Scripts/run.sh vbox
	
debug:
	Scripts/run.sh debug_qemu
