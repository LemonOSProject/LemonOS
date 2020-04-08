@echo off

cd "%~dp0%"

del mount.cfg

cd ..
echo %cd%\Disks\Lemon.vhd
echo sel vdisk file="%cd%\Disks\Lemon.vhd" > build-win\mount.cfg
cd "%~dp0%"
echo attach vdisk >> mount.cfg
echo assign letter=X >> mount.cfg

DiskPart /s "%~dp0%mount.cfg"

del mount.cfg
