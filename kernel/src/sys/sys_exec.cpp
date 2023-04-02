#include "syscall.h"

#include <abi/fcntl.h>
#include <abi/process.h>

#include <List.h>
#include <Logging.h>

long sys_execve(le_str_t _filepath, UserPointer<le_str_t> argv, UserPointer<le_str_t> envp) {
    String filepath = get_user_string_or_fault(_filepath, PATH_MAX);

    List<String> args;
    List<String> environ;

    le_str_t argp;
    do {
        argv.GetValue(argp);
        argv++;

        if(argp) {
            args.add_back(get_user_string_or_fault(argp, SYS_ARGLEN_MAX));
        }

        Log::Info("Arg %s", args.get_back().c_str());
    } while(argp);

    do {
        envp.GetValue(argp);
        envp++;

        if(argp) {
            environ.add_back(get_user_string_or_fault(argp, SYS_ARGLEN_MAX));
        }

        Log::Info("EnV %s", environ.get_back().c_str());
    } while(argp);

    return ENOSYS;
}