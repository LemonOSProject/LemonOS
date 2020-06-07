@echo off

cd "%~dp0%"
echo "%1"
cmd /c mount.bat
cd ..\..
copy initrd.tar L:\Lemon\initrd.tar
copy Kernel\build\kernel.sys L:\Lemon\kernel64.sys
cd Scripts\build-win
cmd /c unmount.bat
