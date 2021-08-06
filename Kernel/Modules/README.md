# Lemon Kernel Modules



## Ext2 (ext2fs.sys)

Ext2 Filesystem Driver

## HDAudio (hdaudio.sys, WIP)
Intel HD Audio Driver (Work in progress)

## Intel8254x (e1k.sys)
Intel 8254x/e1000 Ethernet Adapter Driver

## TestModule (testmodule.sys)
Runs in-kernel tests



# Simple Example Module

```c
#include <Module.h>

#include <Logging.h>
#include <Scheduler.h>

void Test1(){
	Scheduler::Yield();
}

int ModuleInit(){
	Log::Info("Hello, Module World!");

	Test1();

	return 0;
}

int ModuleExit(){
	Log::Info("Goodbye, Module World!");

	return 0;
}

DECLARE_MODULE("examplemodule", "Simple example of a kernel module.", ModuleInit, ModuleExit)
```

