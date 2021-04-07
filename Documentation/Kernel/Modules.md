# Lemon Loadable Kernel Modules

## Creating a Module
To declare a module use `DECLARE_MODULE(mname, mdescription, minit, mexit)`.

- `mname` is the name of the module.
- `mdescription` provides a description of the module.
- `minit` is the initialization function of the module. Must take no arguments and return `int`.\
If the return of `minit` is *not zero* then the module will be unloaded on return.\
An example would be if a PCI device driver was unable to locate any such device and was not needed. It can return a code and will be unloaded afterwards.
- `mexit` is the exit function of the module. Must take no arguments and return `int`. \
To ensure that modules clean up cleanly, a return code *not zero* will result in a kernel panic.