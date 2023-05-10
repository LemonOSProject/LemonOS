#ifndef LEMON_ABI_DIRENT_H
#define LEMON_ABI_DIRENT_H

#include <lemon/abi/stat.h>

#include <abi-bits/ino_t.h>

struct le_dirent {
	ino_t d_ino;
	off_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

#endif // LEMON_ABI_DIRENT_H
