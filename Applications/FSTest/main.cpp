#include <stdio.h>
#include <string.h>
#include <math.h>

enum Action{
    ARead,
    AWrite,
};

int main(int argc, char** argv){
    if(argc < 3){
        printf("Usage: %s <read/write> file [contents, contents2]\n", argv[0]);
        return 1;
    }

    int action = ARead;

    if(strcmp(argv[1], "read") == 0){
        action = ARead;
    } else if(strcmp(argv[1], "write") == 0){
        action = AWrite;
    } else {
        printf("Unknown action: %s\n");
        return 2;
    }

    FILE* file;
    if(!(file = fopen(argv[2], "w"))){
        printf("Failed to open file %s: ", argv[2]);
        perror("");
        return 3;
    }

    if(action == ARead){
        char c;
        while((c = fgetc(file)) != EOF){
            fputc(c, stdout);
        }

        fclose(file);
        return 0;
    } else if(action == AWrite) {
        if(argc > 4){
            for(int i = 3; i < argc; i++){
                fwrite(argv[i], strlen(argv[i]), 1, file);
                fwrite(" ", 1, 1, file);
            }
        } else {
            printf("Warning: no content specified writing a random number\n");

            char str[30];
            sprintf(str, "%d", rand());
            fwrite(str, strlen(str), 1, file);
        }
        
        fclose(file);
        return 0;
    }

    return 0;
}