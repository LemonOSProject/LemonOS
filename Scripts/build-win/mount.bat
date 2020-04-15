@echo off

cd "%~dp0%"

cd ../..
echo %cd%\Disks\Lemon.vhd
echo sel vdisk file="%cd%\Disks\Lemon.vhd" > "%~dp0%mount.cfg"
cd "%~dp0%"
echo attach vdisk >> mount.cfg

DiskPart /s "%~dp0%mount.cfg"
