#include <ctype.h>

int atoi(const char* str){
	int num;
	while (isdigit(*str))
		num = num * 10 + (*str++ - '0');

	return num;
}