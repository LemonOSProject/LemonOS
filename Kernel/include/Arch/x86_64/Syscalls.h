#pragma once

#include <CPU.h>

#include <ABI/Syscall.h>
#define NUM_SYSCALLS 111

#define SC_ARG0(r) ((r)->rdi)
#define SC_ARG1(r) ((r)->rsi)
#define SC_ARG2(r) ((r)->rdx)
#define SC_ARG3(r) ((r)->r10)
#define SC_ARG4(r) ((r)->r9)
#define SC_ARG5(r) ((r)->r8)

void DumpLastSyscall(struct Thread*);

typedef long (*syscall_t)(RegisterContext*);
