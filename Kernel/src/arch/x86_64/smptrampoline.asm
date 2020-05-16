BITS 16

%include "smpdefines.inc"

cli
cld

mov ax, SMP_MAGIC
mov word [SMP_TRAMPOLINE_DATA_START_FLAG], ax

mov eax, cr4
or eax, 1 << 5  ; Set PAE bit
mov cr4, eax

mov eax, dword [SMP_TRAMPOLINE_CR3]
mov cr3, eax

mov ecx, 0xC0000080 ; EFER Model Specific Register
rdmsr               ; Read from the MSR 
or eax, 1 << 8
wrmsr

mov eax, cr0
or eax, 0x80000001 ; Paging, Protected Mode
mov cr0, eax

lgdt [SMP_TRAMPOLINE_GDT_PTR]

jmp 0x08:(smpentry64 + SMP_TRAMPOLINE_ENTRY)

hlt

BITS 64

smpentry64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, [SMP_TRAMPOLINE_STACK]

    mov rax, cr0
	and ax, 0xFFFB		; Clear coprocessor emulation
	or ax, 0x2			; Set coprocessor monitoring
	mov cr0, rax

	;Enable SSE
	mov rax, cr4
	or ax, 3 << 9		; Set flags for SSE
	mov cr4, rax

    xor rbp, rbp
    mov rdi, [SMP_TRAMPOLINE_CPU_ID]
    call [SMP_TRAMPOLINE_ENTRY2]

    cli
    hlt