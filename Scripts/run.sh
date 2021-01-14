if [ -z $LEMOND ]; then
	export LEMOND=$(dirname $(readlink -f "$0"))/..
fi

qemu(){
	qemu-system-x86_64 --enable-kvm $LEMOND/Disks/Lemon.vhd -no-reboot -no-shutdown -m 512M -device qemu-xhci -M q35 -smp 2 -serial stdio -netdev user,id=net0 -device e1000,netdev=net0,mac=DE:AD:69:BE:EF:42
}

qemuefi(){
	qemu-system-x86_64 --enable-kvm --bios /usr/share/edk2-ovmf/x64/OVMF.fd -M q35 -m 512M -drive file=Disks/Lemon.vhd,id=nvme0 -device nvme,serial=deadbeef69,id=nvme0 -serial stdio
}

qemuusb(){
	qemu-system-x86_64 --enable-kvm -drive if=none,id=usbd,file=$LEMOND/Disks/Lemon.vhd -no-reboot -no-shutdown -m 512M -device qemu-xhci,id=xhci -device usb-storage,bus=xhci.0,drive=usbd -M q35 -smp 2 -serial stdio -netdev user,id=net0 -device e1000,netdev=net0,mac=DE:AD:69:BE:EF:42
}


vbox(){
	VBoxManage startvm "Lemon"
}

debug_qemu(){
	echo "target remote localhost:1234" > $(dirname $(readlink -f "$0"))/debug.gdb
	echo "symbol-file $LEMOND/Kernel/build/kernel.sys" >> $(dirname $(readlink -f "$0"))/debug.gdb
	qemu-system-x86_64 -s -S Disks/Lemon.vhd -no-reboot -no-shutdown --enable-kvm -chardev stdio,id=gdb0 -device isa-debugcon,iobase=0x402,chardev=gdb0,id=d1 -d int -m 512M -M q35 -smp 2 -netdev user,id=net0 -device e1000,netdev=net0,mac=DE:AD:69:BE:EF:42&
	gdb -x $(dirname $(readlink -f "$0"))/debug.gdb
}

if [ -z $1 ]; then
	qemu
else
	$1
fi
