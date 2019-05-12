#ifndef STDIO_H
#define STDIO

#define BUFSIZE 1024

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#include <stdarg.h>
#include <stddef.h>

typedef struct IOFILE FILE;

extern FILE const* stderr;

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C"{
#endif

FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);

size_t fread(void* buffer, size_t size, size_t count, FILE* stream);
int fflush(FILE* stream);
int fseek(FILE* stream, long int offset, int whence);

#warning "A large part of stdio is not implemented"

// None of these are implemented
int fprintf(FILE*, const char*, ...);
long ftell(FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
void setbuf(FILE*, char*);
int vfprintf(FILE*, const char*, va_list);

#ifdef __cplusplus
}
#endif

#endif