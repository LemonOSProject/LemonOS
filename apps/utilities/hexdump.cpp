#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <ctype.h>

size_t limit = __UINT64_MAX__;
size_t offset = 0;

int main(int argc, char** argv){
    size_t lengthopt = __UINT64_MAX__;
    int opt;
    while((opt = getopt(argc, argv, "s:n:")) >= 0){
        switch(opt){
            case 's':
                offset = strtoul(optarg, NULL, 10);
                break;
            case 'n':
                lengthopt = strtoul(optarg, NULL, 10);
                break;
            case '?':
                printf("Invalid option: '%c'\nUsage: %s [-n length] [-s skip] [file ...]", opt, argv[0]);
                return 2;
        }
    }

    if(argc - optind < 1){
        printf("Usage: %s [-n length] [-s skip] <file> [file2 ...]", argv[0]);
        return 1;
    }

    if(lengthopt != __UINT64_MAX__)
        limit = offset + lengthopt;

    for(int i = optind; i < argc; i++){
        FILE* f = fopen(argv[i], "rb");

        if(!f){
            fprintf(stderr, "Failed to open %s for reading: %s", argv[i], strerror(errno));
            return 1;
        }

        setvbuf(f, nullptr, _IONBF, 0);
        fseek(f, offset, SEEK_SET);

        unsigned char buf[512];

        size_t len;
        while((len = fread(buf, 1, (((limit - offset) < 512) ? (limit - offset) : 512), f)) && offset < limit){
            for(unsigned j = 0; j < (len + 15) / 16; j++){
                printf("%08lx", offset);
                for(size_t i = 0; i < 16; i++){
                    printf(" %02x", buf[j * 16 + i]);
                }

                printf(" | ");

                for(size_t i = 0; i < 16; i++){
                    if(isprint(buf[j * 16 + i]))
                        printf("%c", buf[j * 16 + i]);
                    else
                        printf(".");
                }

                printf("\n");

                offset += 16;
            }
        }
    }

    return 0;
}