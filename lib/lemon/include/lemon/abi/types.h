#ifndef LEMON_ABI_TYPES_H
#define LEMON_ABI_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef int handle_t;

typedef int le_handle_t;
typedef const char* le_str_t;

typedef int64_t ssize_t;

typedef int pid_t;

typedef unsigned int gid_t;
typedef unsigned int uid_t;

typedef int64_t blksize_t;
typedef int64_t blkcnt_t;

typedef int mode_t;
typedef int nlink_t;

typedef uint64_t dev_t;
typedef int64_t ino_t;
typedef int64_t off_t;

typedef int64_t volume_id_t;

#endif // LEMON_SYS_ABI_TYPES_H
