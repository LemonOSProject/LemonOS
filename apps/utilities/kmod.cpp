#include <lemon/system/Module.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv){
    if(argc != 3){
        printf("Usage: %s <command> [arguments]\n\
kmod load <path>\n\
kmod unload <name>\n", argv[0]);
        return 1;
    }

    if(!strcmp(argv[1], "load")){
        return Lemon::LoadKernelModule(argv[2]);
    } else if(!strcmp(argv[1], "unload")){
        return Lemon::UnloadKernelModule(argv[2]);
    } else {
        printf("Unknown argument: %s\n", argv[1]);
        return 2;
    }
}