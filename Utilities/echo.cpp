#include <stdio.h>

int main(int argc, char** argv){
    for(int i = 1; i < argc; i++){
        printf("%s", argv[i]);

        if(i < argc - 1){ // Print a space at the end
            putc(' ', stdout);
        }
    }

    return 0;
}