@echo off

cd "%~dp0%"
echo "%1"
cmd /c mount.bat
cd ..
copy initrd.img X:\Lemon\initrd.img
copy Kernel\bin\x86\kernel.sys X:\Lemon\kernel.sys
copy Kernel\bin\x86_64\kernel.sys X:\Lemon\kernel64.sys
cd build-win
cmd /c unmount.bat
