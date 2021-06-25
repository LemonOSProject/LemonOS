#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <vector>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <string>

int help = 0;
int inode = 0;
int size = 0;
int listDirectories = 0;
int recursive = 0;

void DisplayEntry(const char* path, struct stat& st){
    std::string entryString = "";

    if(S_ISDIR(st.st_mode)){
        entryString += "\e[95m"; // Magneta foregrouud
    } else if(S_ISLNK(st.st_mode)){
        entryString += "\e[93m"; // Orange foregrouud
    } else if(S_ISCHR(st.st_mode)){
        entryString += "\e[96m"; // Cyan foreground
    } else if(S_ISBLK(st.st_mode)){
        entryString += "\e[94m"; // Blue foreground
    }  else {
        entryString += "\e[31m"; // Red foregrouud
    }

    entryString += path;

    if(size){
        printf("\e[0m%08lu K  %s\n", st.st_size / 1024, entryString.c_str());
    } else {
        printf("%s\n", entryString.c_str());
    }
}

void DisplayPath(const char* path, bool displayPath = false){
    if(listDirectories){
        struct stat sResult;
        stat(path, &sResult);
        DisplayEntry(path, sResult);
    } else {
        struct stat sResult;
        int ret = stat(path, &sResult);
        if(ret){
            fprintf(stderr, "ls: %s: %s", path, strerror(errno));
            return;
        }

        if(!S_ISDIR(sResult.st_mode)){
            DisplayEntry(path, sResult);
            return;
        } else {
            if(displayPath)
                printf("%s:\n", path);

            struct dirent** entries;
            int entryCount = scandir(path, &entries, static_cast<int(*)(const dirent*)>([](const dirent* d) -> int { return strcmp(d->d_name, "..") && strcmp(d->d_name, "."); }), static_cast<int(*)(const dirent**, const dirent**)>([](const dirent** a, const dirent**b) { return strcmp((*a)->d_name, (*b)->d_name); }));

            int i = 0;
            while(i < entryCount){
                struct dirent* dirent = entries[i++];

                if(!strlen(dirent->d_name)) continue;

                if(recursive){
                    std::string dirPath = path;
                    dirPath.append("/");
                    dirPath.append(dirent->d_name);

                    DisplayPath(dirPath.c_str(), true);
                } else {
                    std::string dirPath = path;
                    dirPath.append("/");
                    dirPath.append(dirent->d_name);
                    struct stat sResult;
                    stat(dirPath.c_str(), &sResult);
                    DisplayEntry(dirent->d_name, sResult);
                }
            }
        }
    }
}

int main(int argc, char** argv){
    
    option opts[] = {
        {"help", no_argument, &help, true},
        {"inode", no_argument, nullptr, 'i'},
        {"size", no_argument, nullptr, 's'},
        {"recursive", no_argument, nullptr, 'R'},
    };
    
    int option;
    while((option = getopt_long(argc, argv, "isdR", opts, nullptr)) >= 0){
        switch(option){
            case 'i':
                inode = true;
                break;
            case 's':
                size = true;
                break;
            case 'd':
                listDirectories = true;
                break;
            case 'R':
                recursive = true;
                break;
            case '?':
                printf("For usage see '%s --help'", argv[0]);
                return 1;
        }
    }

    if(help){
        printf("Usage: %s [options] [directory] [directory2] ...\n\
            --help            Show this help\n\
            -i, --inode       Show file inode numbers\n\
            -R, --recursive   Recursively show directory contents\n\
            -s, --size        Show file sizes\n", argv[0]);
        return 1;
    }

    if(optind >= argc){
        DisplayPath(".", false);
    } else for(; optind < argc; optind++){
        DisplayPath(argv[optind], true);
    }

    return 0;
}