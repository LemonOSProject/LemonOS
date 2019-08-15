#include <stdio.h>
#include <string.h>

char* promptStringStart = "\nLemon: ";
char* promptString = ">";
char currentDirectory[128];

extern "C"
int main(int argc, char** argv){

	strcpy(currentDirectory, "/");

	char input;
	char null = 0;
	char buffer[256];

	for(;;){
		puts(promptStringStart);
		puts(currentDirectory);
		puts(promptString);
		while(input != '\n') {
			if(fread(&input,1,1,stdin)){
				puts("test1234\n\n");
			}
		}
	}

	for(;;);
}
