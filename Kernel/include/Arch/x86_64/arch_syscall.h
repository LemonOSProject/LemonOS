#pragma once

#define SC_NUM(r) ((r)->rax)
#define SC_RET(r) ((r)->rax)

#define SC_ARG0(r) ((r)->rdi)
#define SC_ARG1(r) ((r)->rsi)
#define SC_ARG2(r) ((r)->rdx)
#define SC_ARG3(r) ((r)->r10)
#define SC_ARG4(r) ((r)->r9)
#define SC_ARG5(r) ((r)->r8)
