#include <stdio.h>

#include "stdio_internal.h"

FILE _stderr = {
	0,0,0,0,0,2,0,0
};

FILE _stdout = {
	0,0,0,0,0,1,0,0
};

FILE _stdin = {
	0,0,0,0,0,0,0,0
};

FILE* stderr = &_stderr;
FILE* stdout = &_stdout;
FILE* stdin = &_stdin;