#ifndef STDDEF_H
#define STDDEF_H

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

typedef unsigned size_t;

#endif