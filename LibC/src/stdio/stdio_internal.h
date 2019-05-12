#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

struct IOFILE{
    unsigned flags;
    unsigned char *wend, *wpos;

	unsigned char *buf;
	size_t buf_size;
	int fd;
	int mode;

	size_t off;
};