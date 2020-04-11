#include <stdio.h>
#include <unistd.h>
#include <string.h>

FILE* _stdout;
FILE* _stdin;

int main(){
	/*_stdout = fopen("/dev/pty0","rw");
	stdout = _stdout;*/
	//_stdin = fopen("/dev/mouse0","r");
	//stdin = _stdin;

	printf("test string");

	for(;;);
}