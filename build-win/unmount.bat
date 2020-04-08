@echo off

cd "%~dp0%"

del mount.cfg

cd ..
echo sel vdisk file="%cd%\Disks\Lemon.vhd" > build-win\mount.cfg
cd "%~dp0%"
echo detach vdisk >> mount.cfg

DiskPart /s "%~dp0%mount.cfg"

del mount.cfg