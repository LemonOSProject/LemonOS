#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <vector>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <unistd.h>

int help = 0;
int recursive = 0;

int main(int argc, char** argv){
    
    option opts[] = {
        {"help", no_argument, &help, true},
        {"recursive", no_argument, nullptr, 'r'},
    };
    
    int option;
    while((option = getopt_long(argc, argv, "isdrR", opts, nullptr)) >= 0){
        switch(option){
            case 'R':
			case 'r':
                recursive = true;
                break;
            case '?':
                printf("For usage see '%s --help'\n", argv[0]);
                return 1;
        }
    }

    if(help){
        printf("Usage: %s [options] [file] [file2] ...\n\
            --help            Show this help\n\
            -R, --recursive   Recursively delete files\n", argv[0]);
        return 1;
    }

    if(optind >= argc){
        printf("%s: missing arguments\n", argv[0]);
        printf("For usage see '%s --help'\n", argv[0]);
    } else for(; optind < argc; optind++){
        int ret = unlink(argv[optind]);
        if(ret){
            fprintf(stderr, "%s: '%s': %s", argv[0], argv[optind], strerror(errno));
        }
    }

    return 0;
}