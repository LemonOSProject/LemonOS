#include <stdio.h>
#include <stdarg.h>

#warning "WARN: fprintf is a stub"

int fprintf(FILE *restrict file, const char * fmt, ...){
    
}

int vfprintf(FILE* stream, const char* fmt, va_list arg){
	fputs(fmt, stream);
}

int printf(const char * fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return ret;
}