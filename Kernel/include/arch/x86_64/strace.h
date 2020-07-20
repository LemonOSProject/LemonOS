#pragma once

#include <logging.h>
#include <paging.h>

inline static void PrintStackTrace(uint64_t _rbp){
	uint64_t* rbp = (uint64_t*)_rbp;
	uint64_t rip = 0;
	while(rbp){
		rip = *(rbp + 1);
		Log::Info(rip);
		rbp = (uint64_t*)(*rbp);
	}
}

inline static void UserPrintStackTrace(uint64_t _rbp, address_space_t* addressSpace){
	uint64_t* rbp = (uint64_t*)_rbp;
	uint64_t rip = 0;
	while(rbp && Memory::CheckUsermodePointer((uintptr_t)rbp, 16, addressSpace)){
		rip = *(rbp + 1);
		Log::Info(rip);
		rbp = (uint64_t*)(*rbp);
	}
}