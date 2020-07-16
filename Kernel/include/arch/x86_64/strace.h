#pragma once

#include <logging.h>

inline static void PrintStackTrace(uint64_t _rbp){
		uint64_t* rbp = (uint64_t*)_rbp;
		uint64_t rip = 0;
		while(rbp){
			rip = *(rbp + 1);
			Log::Info(rip);
			rbp = (uint64_t*)(*rbp);
		}
}