@echo off

cd "%~dp0%"

del mount.cfg

echo sel vdisk file="%~dp0%Disks\Lemon.vhd" > mount.cfg
echo detach vdisk >> mount.cfg

DiskPart /s "%~dp0%mount.cfg"