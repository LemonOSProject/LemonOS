#pragma once

#define ALWAYS_INLINE __attribute__(( always_inline )) inline

#include <stddef.h>
ALWAYS_INLINE void* operator new(size_t, void* p){
	return p;
}
