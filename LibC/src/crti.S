.section .init
.global _init

_init:
    push %rbp
    mov %rsp, %rbp

.section .fini
.global _fini
_fini:
    push %rbp
    mov %rsp, %rbp