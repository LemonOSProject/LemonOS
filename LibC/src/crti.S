.section .init
.global _init

_init:
    push %ebp
    mov %esp, %ebp

.section .fini
.global _fini
_fini:
    push %ebp
    mov %esp, %ebp