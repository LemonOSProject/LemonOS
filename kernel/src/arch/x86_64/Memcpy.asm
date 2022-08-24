BITS 64

global user_memcpy
global user_memcpy_trap
global user_memcpy_trap_handler
global user_strlen
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
; RAX - return value
; RCX (CL)
user_strlen:
    mov rax, rdi
user_strlen_trap:
    mov cl, byte [rax]
    inc rax
    test cl, cl
    jnz user_strlen_trap

    ; Get difference between RAX and RDI,
    ; Thats the string length
    sub rax, rdi
    dec rax
    ret
user_strlen_trap_handler:
    mov rax, 1
    ret