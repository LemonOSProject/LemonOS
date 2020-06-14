@echo off

cd "%~dp0%"
cd ../..
echo sel vdisk file="%cd%\Disks\Lemon.vhd" > "%~dp0%mount.cfg"
cd "%~dp0%"
echo detach vdisk >> mount.cfg

DiskPart /s "%~dp0%mount.cfg"