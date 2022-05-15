#include <Lemon/Core/Logger.h>

#include <cassert>
#include <mutex>
#include <iostream>
#include <string>

#include <libgen.h>
#include <sys/auxv.h>

namespace Lemon {
namespace Logger {

static const char* programName = nullptr;

const char* GetProgramName(){
    if(programName == nullptr){
        if(peekauxval(AT_EXECPATH, reinterpret_cast<uintptr_t*>(&programName))){
            programName = "unknown";
        } else {
            assert(programName);

            char buf[PATH_MAX];
            strcpy(buf, programName);

            // Give basename a copy of the exec path
            // Then make a copy of the basename
            programName = strdup(basename(buf));
        }
    }

    assert(programName);

    return programName;
}

void Log(short arg){
    std::cerr << arg;
}

void Log(int arg){
    std::cerr << arg;
}

void Log(long arg){
    std::cerr << arg;
}

void Log(unsigned short arg){
    std::cerr << arg;
}

void Log(unsigned int arg){
    std::cerr << arg;
}

void Log(unsigned long arg){
    std::cerr << arg;
}

void Log(float arg){
    std::cerr << arg;
}

} // namespace Logger
} // namespace Lemon