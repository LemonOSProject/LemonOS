#include <stddef.h>
#include <stdlib.h>

__attribute__((weak))
void *operator new(long unsigned int size)
{
    if (size == 0)
        size = 1;
	return malloc(size);
}

__attribute__((weak))
void *operator new[](long unsigned int size)
{
	return malloc(size);
}

__attribute__((weak))
void operator delete(void *p)
{
	free(p);
}

__attribute__((weak))
void operator delete(void *p, long unsigned int)
{
	::operator delete(p);
}

__attribute__((weak))
void operator delete[](void *p)
{
	free(p);
}

__attribute__((weak))
void operator delete[](void *p, long unsigned int)
{
	free(p);
}