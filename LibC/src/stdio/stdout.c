#include <stdio.h>

int putchar(int c){
	return fputc(c, stdout);
}

int puts(const char* s){
	return fputs(s, stdout);
}

