#include <Logging.h>

#include <Scheduler.h>

extern "C"{

void Test1(){
	Scheduler::Yield();
}

void Test2(){
	Scheduler::Yield();
}

void Test3(){
	Scheduler::Yield();
}

void Test4(){
	Scheduler::Yield();
}

int ModuleMain(){
	Log::Info("Hello, Module World!");

	Test1();
	Test2();
	Test3();
	Test4();
	for(;;);
}

}