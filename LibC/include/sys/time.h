#ifndef TIME_H
#define TIME_H

typedef long time_t;

struct timeval{
	time_t tv_sec;
	long tv_usec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

#ifdef __cplusplus
extern "C"{
#endif

int gettimeofday (struct timeval*, void*);

#ifdef __cplusplus
}
#endif

#endif // TIME_H