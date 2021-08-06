#include <Module.h>

#include <Logging.h>

#include "Tests.h"

#define TEST_COUNT 2
Test tests[TEST_COUNT]{
    StringTest,
	ThreadingTest,
};

static int ModuleInit(){
	Log::Info("Hello, Module World!");

	for(unsigned i = 0; i < TEST_COUNT; i++){
		int ret = tests[i]();
		if(ret != 0){
			Log::Info("[TestModule] Failed with code %d", ret);
		}
	}

	return 0;
}

static int ModuleExit(){
	Log::Info("Goodbye, Module World!");

	return 0;
}

DECLARE_MODULE("testmodule", "Runs kernel tests.", ModuleInit, ModuleExit)
