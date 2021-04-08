# Lemon Kernel
This is Lemon OS's Kernel.

**Kernel Modules** are located in [Modules](Modules/)

`modules.cfg` is copied to the initrd. At boot the kernel reads a list of filepaths from `modules.cfg` and loads the respective modules.