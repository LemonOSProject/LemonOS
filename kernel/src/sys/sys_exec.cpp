#include "syscall.h"

#include <abi/fcntl.h>
#include <abi/process.h>

#include <Fs/Filesystem.h>

#include <Objects/Process.h>

#include <List.h>
#include <Logging.h>

SYSCALL long sys_execve(le_str_t _filepath, UserPointer<le_str_t> argv, UserPointer<le_str_t> envp) {
    String filepath = get_user_string_or_fault(_filepath, PATH_MAX);

    Vector<String> args;
    Vector<String> environ;

    le_str_t argp;
    do {
        if(argv.get(argp))
            return EFAULT;
        argv++;

        if (argp) {
            args.add_back(get_user_string_or_fault(argp, SYS_ARGLEN_MAX));
        }
    } while (argp);

    do {
        if(envp.get(argp))
            return EFAULT;
        envp++;

        if (argp) {
            environ.add_back(get_user_string_or_fault(argp, SYS_ARGLEN_MAX));
        }
    } while (argp);

    Process* proc = Process::current();
    auto* inode = fs::ResolvePath(filepath, proc->workingDir->inode);
    if(!inode) {
        return ENOENT;
    }

    FancyRefPtr<File> file = SC_TRY_OR_ERROR(fs::Open(inode, 0));
    if(file->is_directory()) {
        return ENOEXEC;
    }

    elf64_header_t elfHeader;

    UIOBuffer uio{&elfHeader};
    SC_TRY_OR_ERROR(file->read(0, sizeof(elf64_header_t), &uio));

    if(!VerifyELF(&elfHeader)) {
        return ENOEXEC;
    }

    ELFData exe;
    if(elf_load_file(file, exe) != ERROR_NONE) {
        return ENOEXEC;
    }

    proc->kill_all_other_threads();

    for(le_handle_t i = 0; i < proc->handle_count(); i++) {
        Handle h = proc->get_handle(i);

        if(h.IsValid() && h.closeOnExec) {
            proc->handle_destroy(i);
        }
    }

    if(proc->execve(exe, args, environ, filepath.c_str()) != ERROR_NONE) {
        proc->die();
    }

    elf_free_data(exe);

    return 0;
}
