extern GDT64
extern GDT64.TSS
extern GDT64.TSS.low
extern GDT64.TSS.mid
extern GDT64.TSS.high
extern GDT64.TSS.high32

global LoadTSS

LoadTSS: ; RDI - address, RSI - GDT, RDX - selector
    push rbx ; Save RBX
    
    lea rbx, [rsi + rdx] ; Length of TSS
    mov word [rbx], 108

    mov eax, edi
    lea rbx, [rsi + rdx + 2] ; Low Bytes of TSS
    mov word [rbx], ax

    mov eax, edi
    shr eax, 16
    lea rbx, [rsi + rdx + 4] ; Mid
    mov byte [rbx], al

    mov eax, edi
    shr eax, 24
    lea rbx, [rsi + rdx + 7] ; High
    mov byte [rbx], al

    mov rax, rdi
    shr rax, 32
    lea rbx, [rsi + rdx + 8] ; High 32
    mov dword [rbx], eax

    pop rbx ; Restore RBX
    ret