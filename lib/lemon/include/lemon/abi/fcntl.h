#ifndef LEMON_SYS_ABI_FCNTL_H
#define LEMON_SYS_ABI_FCNTL_H

#define PATH_MAX 4096
#define NAME_MAX 255

#define O_ACCESS 0x0007
#define O_EXEC 1
#define O_RDONLY 2
#define O_RDWR 3
#define O_SEARCH 4
#define O_WRONLY 5

#define O_APPEND    0x000008
#define O_CREAT     0x000010
#define O_DIRECTORY 0x000020
#define O_EXCL      0x000040
#define O_NOCTTY    0x000080
#define O_NOFOLLOW  0x000100
#define O_TRUNC     0x000200
#define O_NONBLOCK  0x000400
#define O_DSYNC     0x000800
#define O_RSYNC     0x001000
#define O_SYNC      0x002000
#define O_CLOEXEC   0x004000
#define O_PATH      0x008000
#define O_LARGEFILE 0x010000
#define O_NOATIME   0x020000
#define O_ASYNC     0x040000
#define O_TMPFILE   0x080000
#define O_DIRECT    0x100000

#define F_DUPFD 1
#define F_DUPFD_CLOEXEC 2
#define F_GETFD 3
#define F_SETFD 4
#define F_GETFL 5
#define F_SETFL 6
#define F_GETLK 7
#define F_SETLK 8
#define F_SETLKW 9
#define F_GETOWN 10
#define F_SETOWN 11

#define F_RDLCK 1
#define F_UNLCK 2
#define F_WRLCK 3

#define FD_CLOEXEC 1

#define AT_EMPTY_PATH 1
#define AT_SYMLINK_FOLLOW 2
#define AT_SYMLINK_NOFOLLOW 4
#define AT_REMOVEDIR 8
#define AT_EACCESS 512

#define AT_FDCWD -100

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

#endif // LEMON_SYS_ABI_FCNTL
