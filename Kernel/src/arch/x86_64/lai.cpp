/* Provides Helper Functions and Abstractions for LAI */

#include <logging.h>
#include <panic.h>

extern "C"{
	void laihost_log(int i, const char* s){
		i ? Log::Warning((char*)s) : Log::Info((char*)s);
	}

	void laihost_panic(const char* s){
		char* reasons[]{"ACPI Error",(char*)s};
		KernelPanic(reasons,2);
	}
}