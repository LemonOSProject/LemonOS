#ifndef STDIO_H
#define STDIO

#define BUFSIZE 1024

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define EOF -1

#include <stdarg.h>
#include <stddef.h>

typedef struct IOFILE FILE;

extern FILE* stderr;
extern FILE* stdout;
extern FILE* stdin;

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C"{
#endif

FILE* fdopen(int fd, const char* mode);
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);

size_t fread(void* buffer, size_t size, size_t count, FILE* stream);
size_t fwrite(const void*, size_t, size_t, FILE*);
int fflush(FILE* stream);
int fseek(FILE* stream, long int offset, int whence);
long ftell(FILE* stream);

int fputc(int c, FILE* f);
int fputs(const char* str, FILE* f);

int putchar(int c);
int puts(const char* s);
int vfprintf(FILE*, const char*, va_list);
int printf(const char * fmt, ...);

#warning "A large part of stdio is not implemented"

// None of these are implemented
int fprintf(FILE*, const char*, ...);
void setbuf(FILE*, char*);
int feof(FILE* f);

#ifdef __cplusplus
}
#endif

#endif