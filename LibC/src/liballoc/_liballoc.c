#include <core/syscall.h>
#include <stdbool.h>
#include <stddef.h>

bool lock = false;

int liballoc_lock() {
	/*if(!lock){
		lock = true;
		return 0;
	} else {
		return 1;
	}*/
	return 0;
}

int liballoc_unlock() {
	lock = false;
	return 0;
}

void* liballoc_alloc(int pages) {
	void* addr;
	syscall(SYS_MEMALLOC, pages, (uint32_t)&addr, 0, 0, 0);
	return addr;
}

int liballoc_free(void* addr, size_t pages) {
	// Implement Memory freeing
	return 0;
}
