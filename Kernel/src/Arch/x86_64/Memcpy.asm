BITS 64

global UserMemcpy
global UserMemcpyTrap
global UserMemcpyTrapHandler

; UserMemcpy (dst, src, cnt)
UserMemcpy:
    mov rcx, rdx

UserMemcpyTrap:
    rep movsb

    mov rax, 0
    ret
UserMemcpyTrapHandler:
    mov rax, 1
    ret
