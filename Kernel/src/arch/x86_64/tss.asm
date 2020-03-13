extern GDT64
extern GDT64.TSS
extern GDT64.TSS.low
extern GDT64.TSS.mid
extern GDT64.TSS.high
extern GDT64.TSS.high32

global LoadTSS

LoadTSS: ; RDI - address 
    push rbx ; Save RBX

    mov eax, edi
    mov rbx, GDT64.TSS + 2 ; Low Bytes of TSS
    mov word [rbx], ax

    mov eax, edi
    shr eax, 16
    mov rbx, GDT64.TSS + 4 ; Mid
    mov byte [rbx], al

    mov eax, edi
    shr eax, 24
    mov rbx, GDT64.TSS + 7 ; High
    mov byte [rbx], al

    mov rax, rdi
    shr rax, 32
    mov rbx, GDT64.TSS + 8 ; High 32
    mov dword [rbx], eax

    pop rbx ; Restore RBX
    ret