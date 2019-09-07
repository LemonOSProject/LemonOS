/* Provides Helper Functions and Abstractions for LAI */

#include <logging.h>
#include <panic.h>
#include <paging.h>
#include <timer.h>

extern "C"{
	void laihost_log(int i, const char* s){
		i ? Log::Warning((char*)s) : Log::Info((char*)s);
	}

	void laihost_panic(const char* s){
		char* reasons[]{"ACPI Error",(char*)s};
		KernelPanic(reasons,2);
	}

	void* laihost_map(uintptr_t address, uint64_t count){
		uint64_t pages = count / PAGE_SIZE_2M + 1;

		void* addr = Memory::KernelAllocate2MPages(pages);
		Memory::KernelMapVirtualMemory2M(address, (uint64_t)addr, pages);

		return addr;
	}

	void laihost_sleep(uint64_t ms){
		uint64_t freq = Timer::GetFrequency();
		float delayInTicks = (freq/1000)*ms;
		uint64_t seconds = Timer::GetSystemUptime();
		uint64_t ticks = Timer::GetTicks();
		uint64_t totalTicks = seconds*freq + ticks;

		for(;;){
			uint64_t totalTicksNew = Timer::GetSystemUptime()*freq + Timer::GetTicks();
			if(totalTicksNew - totalTicks == (int)delayInTicks){
				break;
			}
		}
		return;
	}
}