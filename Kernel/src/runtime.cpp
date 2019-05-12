#pragma once

#include <stddef.h>
#include <memory.h>

#include <memory.h>
#include <stddef.h>

void *operator new(size_t size)
{
	return kmalloc(size);
}

void *operator new[](size_t size)
{
	return kmalloc(size);
}

void operator delete(void *p)
{
	kfree(p);
}

void operator delete(void *p, size_t)
{
	::operator delete(p);
}


void operator delete[](void *p)
{
	kfree(p);
}

void operator delete[](void *p, size_t)
{
	::operator delete[](p);
}
