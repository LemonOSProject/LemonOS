#include <lemon/system/util.h>
#include <lemon/syscall.h>

#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

extern char** environ;

pid_t lemon_spawn(const char* path, int argc, char* const argv[], int flags){
	return syscall(SYS_EXEC, (uintptr_t)path, argc, (uintptr_t)argv, flags, environ);
} 

pid_t lemon_spawn(const char* path, int argc, char* const argv[], int flags, char** envp){
	return syscall(SYS_EXEC, (uintptr_t)path, argc, (uintptr_t)argv, flags, envp);
} 

namespace Lemon{
    void Yield(){
        syscall(SYS_YIELD);
    }

    int GetProcessInfo(pid_t pid, lemon_process_info_t& pInfo){
        long ret = -1;
        if((ret = syscall(SYS_GET_PROCESS_INFO, pid, &pInfo))){
            errno = -ret;
            return -1;
        }

        return 0;
    }

    int GetNextProcessInfo(pid_t* pid, lemon_process_info_t& pInfo){
        long ret = 1;
        if((ret = syscall(SYS_GET_NEXT_PROCESS_INFO, pid, &pInfo)) < 0){
            errno = -ret;
            return -1;
        }

        return ret;
    }

    void GetProcessList(std::vector<lemon_process_info_t>& list){
        pid_t pid = 0;

        list.clear();

        lemon_process_info_t pInfo;
        while(!GetNextProcessInfo(&pid, pInfo)){
            list.push_back(pInfo);
        }
    }
}