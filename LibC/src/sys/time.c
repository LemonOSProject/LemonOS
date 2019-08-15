#include <core/syscall.h>

typedef long time_t;

struct timeval{
	time_t tv_sec;
	long tv_usec;
};

int gettimeofday (struct timeval* tv, void* tz){
	uint32_t sec;
	uint32_t msec;

	syscall(SYS_UPTIME,(uint32_t)&sec,(uint32_t)&msec,0,0,0);

	tv->tv_sec = sec;
	tv->tv_usec = msec*100;
}