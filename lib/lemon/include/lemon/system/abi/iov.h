#ifndef LEMON_SYS_ABI_IOV_H
#define LEMON_SYS_ABI_IOV_H

#include <stddef.h>

struct iovec {
	void* iov_base;
	size_t iov_len;
};

#endif
