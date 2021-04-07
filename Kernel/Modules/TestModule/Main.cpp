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

DECLARE_MODULE("testmodule", "A test kernel module.", ModuleInit, ModuleExit)