#ifndef LEMON_ABI_TIME_H
#define LEMON_ABI_TIME_H

typedef long time_t;
typedef long suseconds_t;
typedef long clock_t;

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

struct timeval {
	time_t tv_sec;
	suseconds_t tv_usec;
};

#endif