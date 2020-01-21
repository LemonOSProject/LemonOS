#include <lemon/syscall.h>

typedef long time_t;

struct timeval{
	time_t tv_sec;
	long tv_usec;
};

int gettimeofday (struct timeval* tv, void* tz){
	uint64_t sec;
	uint64_t msec;

	syscall(SYS_UPTIME,(uint64_t)&sec,(uint64_t)&msec,0,0,0);

	tv->tv_sec = sec;
	tv->tv_usec = msec*100;
}
