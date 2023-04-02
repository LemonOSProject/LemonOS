# Kaimu Kernel
This is Lemon OS's Kernel. Aims to make huge improvements over the old kernel, mainly rewriting the whole user-kernel syscall boundary, preventing unchecked accesses to user memory.

**Kernel Modules** are located in [Modules](Modules/)

`modules.cfg` is copied to the initrd. At boot the kernel reads a list of filepaths from `modules.cfg` and loads the respective modules.