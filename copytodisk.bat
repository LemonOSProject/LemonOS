cd "%~dp0%"
echo "%1"
cmd /c mount.bat
copy initrd.img X:\Lemon\initrd.img
copy Kernel\bin\x86\kernel.sys X:\Lemon\kernel.sys
cmd /c unmount.bat