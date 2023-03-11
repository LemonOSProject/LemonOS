BITS 64

global user_memcpy
global user_memcpy_trap
global user_memcpy_trap_handler
global user_strnlen
global user_strlen_trap
global user_strlen_trap_handler

; UserMemcpy (dst, src, cnt)
user_memcpy:
    mov rcx, rdx

user_memcpy_trap:
    rep movsb

    xor rax, rax
    ret
user_memcpy_trap_handler:
    mov rax, 1
    ret

; user_strlen (string)
; RDI - string
; RSI - max length
; RAX - return value
; RCX (CL)
user_strnlen:
    mov rax, rdi

    ; Add RDI to RSI so we get the max memory
    ; address the string should reach
    add rsi, rdi

.length_check:
    ; Make sure we stop at the given length
    cmp rax, rsi
    jge user_strlen_trap.ret

user_strlen_trap:
    mov cl, byte [rax]
    inc rax

    test cl, cl
    jnz user_strnlen.length_check

    ; Decrement RAX since we increment it regardless of whether we encounter a zero
    dec rax
.ret:
    ; Get difference between RAX and RDI,
    ; That's the string length
    sub rax, rdi
    ret
user_strlen_trap_handler:
    mov rax, -1
    ret