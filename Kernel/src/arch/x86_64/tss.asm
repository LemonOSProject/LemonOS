extern GDT64
extern GDT64.TSS

global LoadTSS

LoadTSS: ; rdi - address 

    mov rax, GDT64.TSS + 2 ; Low Bytes of TSS
    mov word [rax], di

    shr rdi, 16 ; Shift Address 16-bits

    mov rax, GDT64.TSS + 4 ; Mid
    mov byte [rax], dl

    shr rdi, 8

    mov rax, GDT64.TSS + 7 ; High
    mov byte [rax], dl

    shr rdi, 8

    mov rax, GDT64.TSS + 12 ; High 32
    mov dword [rax], edi

    ret
